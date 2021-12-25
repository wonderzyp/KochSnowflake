cmake_minimum_required(VERSION 3.12...3.15 FATAL_ERROR)
project(KochSnowflake CXX)

#Find the VTK-m package
find_package(VTKm REQUIRED QUIET)

add_executable(main main.cxx myWorklet.h)
target_link_libraries(main PRIVATE vtkm_worklet vtkm_rendering)

vtkm_add_target_information(main
                            DROP_UNUSED_SYMBOLS MODIFY_CUDA_FLAGS
                            DEVICE_SOURCES main.cxx)
