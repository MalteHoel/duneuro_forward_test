add_executable("test_localized_subtraction_eeg" test_localized_subtraction_eeg.cc)
target_link_dune_default_libraries("test_localized_subtraction_eeg")

# check if path to the root directory of simbiosphere was defined
if (NOT DEFINED SIMBIOSPHERE_ROOT)
	message(FATAL_ERROR "path to root directory of simbiosphere not defined as a cmake variable")
endif()

# include and link against simbiosphere
set_property(TARGET "test_localized_subtraction_eeg" APPEND PROPERTY INCLUDE_DIRECTORIES ${SIMBIOSPHERE_ROOT}/src/include)
target_link_libraries("test_localized_subtraction_eeg" ${SIMBIOSPHERE_ROOT}/build/src/libsimbiosphere.so)
