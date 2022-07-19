add_executable("test_localized_subtraction_eeg" test_localized_subtraction_eeg.cc)
target_link_dune_default_libraries("test_localized_subtraction_eeg")

# check if path to the root directory of simbiosphere was defined
if (NOT DEFINED SIMBIOSPHERE_ROOT)
	message(FATAL_ERROR "path to root directory of simbiosphere not defined as a cmake variable")
endif()

# include and link against simbiosphere
set_property(TARGET "test_localized_subtraction_eeg" APPEND PROPERTY INCLUDE_DIRECTORIES ${SIMBIOSPHERE_ROOT}/src/include)
target_link_libraries("test_localized_subtraction_eeg" ${SIMBIOSPHERE_ROOT}/build/src/libsimbiosphere.so)
execute_process(COMMAND ${CMAKE_COMMAND} "-E" "create_symlink" "${CMAKE_CURRENT_SOURCE_DIR}/input/sphere/configs.ini" "${CMAKE_CURRENT_BINARY_DIR}/configs.ini")
execute_process(COMMAND ${CMAKE_COMMAND} "-E" "create_symlink" "${CMAKE_CURRENT_SOURCE_DIR}/input/sphere/mesh.msh" "${CMAKE_CURRENT_BINARY_DIR}/mesh.msh")
execute_process(COMMAND ${CMAKE_COMMAND} "-E" "create_symlink" "${CMAKE_CURRENT_SOURCE_DIR}/input/sphere/conductivities.txt" "${CMAKE_CURRENT_BINARY_DIR}/conductivities.txt")
execute_process(COMMAND ${CMAKE_COMMAND} "-E" "create_symlink" "${CMAKE_CURRENT_SOURCE_DIR}/input/sphere/electrodes.txt" "${CMAKE_CURRENT_BINARY_DIR}/electrodes.txt")
execute_process(COMMAND ${CMAKE_COMMAND} "-E" "create_symlink" "${CMAKE_CURRENT_SOURCE_DIR}/input/sphere/dipoles.txt" "${CMAKE_CURRENT_BINARY_DIR}/dipoles.txt")
