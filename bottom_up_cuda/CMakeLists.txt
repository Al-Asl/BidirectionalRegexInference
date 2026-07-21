cmake_minimum_required(VERSION 3.24)

project(REI LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

function(define_enum_option VAR_NAME DEFAULT_VALUE DESCRIPTION VALUES INDEX)
    # Create the cached variable
    set(${VAR_NAME} "${DEFAULT_VALUE}" CACHE STRING "${DESCRIPTION}")

    # Store the valid values in the cache property for GUI tools
    set_property(CACHE ${VAR_NAME} PROPERTY STRINGS ${VALUES})

    # Show status message with current value and options
    string(REPLACE ";" ", " VALUES_COMMA "${VALUES}")
    message(STATUS "${VAR_NAME} is set to: ${${VAR_NAME}}")
    message(STATUS "Valid values for ${VAR_NAME}: ${VALUES_COMMA}")

    # Validate user input
    list(FIND VALUES "${${VAR_NAME}}" _index)
    if(_index EQUAL -1)
        message(FATAL_ERROR
            "${VAR_NAME} has invalid value: '${${VAR_NAME}}'.\n"
            "Valid options are: ${VALUES_COMMA}"
        )
    endif()
    set(${INDEX} ${_index} PARENT_SCOPE)
endfunction()

message(STATUS "=================== Options ===================")

option(PROFILE_MODE "Show the source code when using Nsight Compute" OFF)
message(STATUS "PROFILE_MODE is set to: ${PROFILE_MODE}")

option(CS_DECOMPOSETION "Show Decomposetion of the output regex" ON)
message(STATUS "CS_DECOMPOSETION is set to: ${CS_DECOMPOSETION}")

set(SOLUTIONS_NUM 1 CACHE STRING "Target number of solutions to search")
message(STATUS "SOLUTIONS_NUM is set to: ${SOLUTIONS_NUM}")

define_enum_option(
    RELAX_UNIQUENESS_CHECK_TYPE           
    "2"                
    "The algorthim for relax uniqueness check" 
    "0;1;2;3"
    RELAX_UNIQUENESS_CHECK_TYPE_INDEX
)

define_enum_option(
    LOG_LEVEL           
    OFF                
    "Logging verbosity" 
    "OFF;DC;REI_BASIC;REI_KERNELS"
    LOG_LEVEL_INDEX
)

message(STATUS "===============================================")

set(HEADERS
include/rei.h 
include/cuda_common.h
include/operations_device.h
include/bottom_up_device.h
include/remove_stride_groups.h
)

set(SOURCES
src/main.cu
src/bottom_up_device.cu
)

find_package(CUDAToolkit REQUIRED)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        ${SOURCES}
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES ${HEADERS}
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE 
    LOG_LEVEL=${LOG_LEVEL_INDEX}
    RELAX_UNIQUENESS_CHECK_TYPE=${RELAX_UNIQUENESS_CHECK_TYPE_INDEX}
    SOLUTIONS_NUM=${SOLUTIONS_NUM}
    $<$<BOOL:${GUIDE_TABLE_CONSTANT_MEMORY}>:GUIDE_TABLE_CONSTANT_MEMORY>
    $<$<BOOL:${CS_DECOMPOSETION}>:CS_DECOMPOSETION>
)

# cuda section
set_property(TARGET ${PROJECT_NAME} PROPERTY CUDA_ARCHITECTURES native)
target_compile_options(${PROJECT_NAME}  PRIVATE  $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>) 
target_link_libraries(${PROJECT_NAME} PRIVATE cudart)
target_link_libraries(${PROJECT_NAME} PRIVATE shared)
if(PROFILE_MODE)
	set(CMAKE_CUDA_FLAGS_RELEASE "${CMAKE_CUDA_FLAGS_RELEASE} --generate-line-info")
endif()

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/modified_libraries)

# Set the startup project for Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})