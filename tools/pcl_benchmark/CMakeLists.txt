cmake_minimum_required(VERSION 3.18)
project(pcl_icp_benchmark CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PCL_DIR "E:/ICPtry/PCL/PCL 1.12.1/cmake" CACHE PATH "PCL cmake dir")
find_package(PCL 1.12 REQUIRED COMPONENTS common io registration)

add_executable(pcl_icp_benchmark pcl_icp_benchmark.cpp)
target_include_directories(pcl_icp_benchmark PRIVATE ${PCL_INCLUDE_DIRS})
target_link_libraries(pcl_icp_benchmark PRIVATE ${PCL_LIBRARIES})
target_compile_definitions(pcl_icp_benchmark PRIVATE ${PCL_DEFINITIONS})
target_compile_options(pcl_icp_benchmark PRIVATE -bigobj)

if(MSVC)
    target_compile_options(pcl_icp_benchmark PRIVATE /MP /W3)
endif()
