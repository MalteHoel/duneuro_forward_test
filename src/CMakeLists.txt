add_executable("test_localized_subtraction" test_localized_subtraction.cc)
target_link_dune_default_libraries("test_localized_subtraction")
set_property(TARGET "test_localized_subtraction" APPEND PROPERTY INCLUDE_DIRECTORIES /home/malte/Desktop/duneuro/localized_subtraction/simbiosphere/src/include)
target_link_libraries("test_localized_subtraction" /home/malte/Desktop/duneuro/localized_subtraction/simbiosphere/build/src/libsimbiosphere.so)
