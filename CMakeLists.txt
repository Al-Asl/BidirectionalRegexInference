cmake_minimum_required(VERSION 3.24)
project(REI)

add_subdirectory(shared)

option(BUILD_BOTTOM_UP_CUDA "Build the CUDA version of the bottom-up project" OFF)
option(BUILD_BIDIRECTIONAL "Build the CPU version of the bidirectional project" OFF)

if(BUILD_BOTTOM_UP_CUDA)
	add_subdirectory(bottom_up_cuda)
endif()

if(BUILD_BIDIRECTIONAL)
	add_subdirectory(bidirectional)
endif()