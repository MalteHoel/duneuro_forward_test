// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <iostream>
#include <dune/common/parallel/mpihelper.hh> // An initializer of MPI
#include <dune/common/exceptions.hh> // We use exceptions

#include <vector>
#include <string>
#include <stdlib.h>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <iterator>
#include <dune/common/parametertreeparser.hh>
#include <dune/common/fvector.hh>
#include <duneuro/driver/driver_factory.hh>
#include <duneuro/io/dipole_reader.hh>
#include <duneuro/common/function.hh>
#include <duneuro/io/point_vtk_writer.hh>
#include <duneuro/io/field_vector_reader.hh>
#include <duneuro/io/projections_reader.hh>
#include <duneuro/common/dense_matrix.hh>
#include <simbiosphere/analytic_solution.hh>              // simbiosphere implements the analytic solution of the EEG forward problem in sphere models


// functions computing norm, relative error, MAG and RDM. Basically copied from 
// https://gitlab.dune-project.org/duneuro/duneuro-tests/-/blob/feature/2.8-changes/src/test_eeg_forward.cc

// compute euclidean norm of a vector
template<class T>
T norm(const std::vector<T>& vector) {
  return std::sqrt(std::inner_product(vector.begin(), vector.end(), vector.begin(), T(0.0)));
}

// compute relative error
template<class T>
  T relative_error(const std::vector<T>& numerical_solution, const std::vector<T>& analytical_solution) {
  std::vector<T> diff;
  std::transform(numerical_solution.begin(), 
                 numerical_solution.end(), 
                 analytical_solution.begin(), 
                 std::back_inserter(diff),
                 [] (const T& num_val, const T& ana_val) {return num_val - ana_val;});
   return norm(diff) / norm(analytical_solution);
}

// compute MAG error
template<class T>
T magnitude_error(const std::vector<T>& numerical_solution, const std::vector<T>& analytical_solution) {
  return norm(numerical_solution) / norm(analytical_solution);
}

// compute RDM error
template<class T>
T relative_difference_measure(const std::vector<T>& numerical_solution, const std::vector<T>& analytical_solution) {
  T norm_numerical = norm(numerical_solution);
  T norm_analytical = norm(analytical_solution);
  std::vector<T> diff;
  std::transform(numerical_solution.begin(),
                 numerical_solution.end(),
                 analytical_solution.begin(),
                 std::back_inserter(diff),
                 [norm_numerical, norm_analytical] (const T& num_val, const T& ana_val)
                   {return num_val / norm_numerical - ana_val / norm_analytical;});
  return norm(diff); 
}

// subtract mean of vector, so that new mean of vector is zero 
template<class T>
void subtract_mean(std::vector<T>& vector) {
  T mean = std::accumulate(vector.begin(), vector.end(), T(0.0)) / vector.size();
  for(T& entry : vector) {
    entry -= mean;
  }
}

// duneuro works with the Dune::FieldVector data structure, while simbiosphere works with the std::array data structure.
// to use simbiosphere we thus need to convert FieldVectors to arrays
template<int dim, size_t dim_unsigned>
void copy_to_array(const Dune::FieldVector<double, dim>& field_vector, std::array<double, dim_unsigned>& array) {
  for(int i = 0; i < dim; ++i) {
    array[i] = field_vector[i];
  }
}

// assumes that the vector to be copied to is empty in the beginning
template<int dim, size_t dim_unsigned>
void copy_to_vector_of_arrays(const std::vector<Dune::FieldVector<double, dim>>& vector_array, std::vector<std::array<double, dim_unsigned>>& vector_field_vector) {
  for(int i = 0; i < vector_array.size(); ++i) {
    std::array<double, dim> current;
    copy_to_array(vector_array[i], current);
    vector_field_vector.push_back(current);
  }
}




int main(int argc, char** argv)
{
  try{
    // Maybe initialize MPI
    Dune::MPIHelper& helper = Dune::MPIHelper::instance(argc, argv);
    
    std::cout << "The goal of this program is to quickly test the EEG forward solver implemented in DUNEuro.\n";
    
    
    // typedefs and constants
    using ScalarType = double;
    constexpr int dim = 3;
    
    
    // read parameter tree
    std::cout << " Reading parameter tree\n";
    Dune::ParameterTree config_tree;
    Dune::ParameterTreeParser config_parser;
     
    config_parser.readINITree("configs.ini", config_tree);
    bool write_output = config_tree.get<bool>("output.write");
    std::cout << " Parameter tree read\n";
    
    
    // create driver
    std::cout << " Creating driver\n";
    using Driver = duneuro::DriverInterface<dim>;
    std::unique_ptr<Driver> driver_ptr = duneuro::DriverFactory<dim>::make_driver(config_tree);
    std::cout << " Driver created\n";
    
    
    // read dipole
    std::cout << " Reading dipoles\n";
    std::vector<duneuro::Dipole<ScalarType, dim>> dipoles = duneuro::DipoleReader<ScalarType, dim>::read(config_tree.get<std::string>("dipole.filename"));
    std::cout << " Dipoles read\n";
    duneuro::Dipole<ScalarType, dim> my_dipole = dipoles[0];
    
    // get EEG forward solution
    std::cout << " Solve EEG forward problem numerically\n";
    std::unique_ptr<duneuro::Function> solution_storage_ptr = driver_ptr->makeDomainFunction();
    driver_ptr->solveEEGForward(my_dipole, *solution_storage_ptr, config_tree);
    
    // evaluate potential at electrode positions
    Dune::ParameterTree electrode_config = config_tree.sub("electrodes");
    std::vector<Dune::FieldVector<ScalarType, dim>> my_electrodes = duneuro::FieldVectorReader<ScalarType, dim>::read(electrode_config.get<std::string>("filename"));
    
    driver_ptr->setElectrodes(my_electrodes, electrode_config);
    std::vector<ScalarType> solution_at_electrode_projections = driver_ptr->evaluateAtElectrodes(*solution_storage_ptr);
    subtract_mean(solution_at_electrode_projections);
    std::cout << " Numerical solution computed\n";
    
    
    
    // compute analytical solution
    std::cout << " Computing analytical solution using simbiosphere\n";
    constexpr int number_of_layers = 4;
    std::array<ScalarType, number_of_layers> radii = config_tree.get<std::array<ScalarType, number_of_layers>>("analytic_solution.radii");
    std::array<ScalarType, dim> center = config_tree.get<std::array<double, dim>>("analytic_solution.center");
    std::vector<Dune::FieldVector<ScalarType, number_of_layers>> conductivities 
      = duneuro::FieldVectorReader<ScalarType, number_of_layers>::read(config_tree.get<std::string>("volume_conductor.tensors.filename"));
    std::array<ScalarType, number_of_layers> conductivities_simbio;
    copy_to_array(conductivities[0], conductivities_simbio);
    

    // store electrodes and dipole in the data structure simbiosphere expects
    std::vector<std::array<ScalarType, dim>> electrodes_simbio;
    copy_to_vector_of_arrays(my_electrodes, electrodes_simbio);

    std::array<ScalarType, dim> dipole_position_simbio;
    copy_to_array(my_dipole.position(), dipole_position_simbio);
    std::array<ScalarType, dim> dipole_moment_simbio;
    copy_to_array(my_dipole.moment(), dipole_moment_simbio);
    

    std::vector<ScalarType> analytical_solution = simbiosphere::analytic_solution(radii, 
                                                                                  center, 
                                                                                  conductivities_simbio, 
                                                                                  electrodes_simbio, 
                                                                                  dipole_position_simbio, 
                                                                                  dipole_moment_simbio);
    subtract_mean(analytical_solution);
    std::cout << " Analytical solution computed\n";
    
    
    // compare numerical and analytical solution
    std::cout << "\n We now compare the analytical and the numerical solution\n";
    
    std::cout << " Norm of analytical solution : " << norm(analytical_solution) << "\n";
    std::cout << " Norm of numerical solution : " << norm(solution_at_electrode_projections) << "\n";
    std::cout << " Relative error : " << relative_error(solution_at_electrode_projections, analytical_solution) << "\n";
    std::cout << " MAG : " << magnitude_error(solution_at_electrode_projections, analytical_solution) << "\n";
    std::cout << " RDM : " << relative_difference_measure(solution_at_electrode_projections, analytical_solution) << "\n";
    
    std::cout << " Comparison finished\n\n";
    
    // visualization
    if(write_output) {
      std::cout << " We now write the solution in the vtk-format\n";
      std::cout << " We first write the headmodel\n";
      auto volume_writer_ptr = driver_ptr->volumeConductorVTKWriter(config_tree);
      volume_writer_ptr->addVertexData(*solution_storage_ptr, "potential");
      volume_writer_ptr->addCellDataGradient(*solution_storage_ptr, "gradient");
      volume_writer_ptr->write(config_tree.sub("output"));
       	
      std::cout << " We now write the dipole\n";
      duneuro::PointVTKWriter<ScalarType, dim> dipole_writer{my_dipole};
      std::string dipole_filename_string = config_tree.get<std::string>("output.filename_dipole");
      dipole_writer.write(dipole_filename_string);
      
      duneuro::PointVTKWriter<ScalarType, dim> potential_writer{my_electrodes};
      
      std::cout << " We now write the potential at the electrodes computed analytically and numerically\n";
      potential_writer.addScalarData("potential_analytical", analytical_solution);
      potential_writer.addScalarData("potential_numerical", solution_at_electrode_projections);
      std::string electrode_potential_filename_string =config_tree.get<std::string>("output.filename_electrode_potentials");
      potential_writer.write(electrode_potential_filename_string);
    }
    
    std::cout << " The program didn't crash!\n";
    
    return 0;

  }
  catch (Dune::Exception &e){
    std::cerr << "Dune reported error: " << e << std::endl;
  }
  catch (...){
    std::cerr << "Unknown exception thrown!" << std::endl;
  }
}
  
  
