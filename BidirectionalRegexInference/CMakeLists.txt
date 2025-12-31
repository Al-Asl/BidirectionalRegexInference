cmake_minimum_required(VERSION 3.24)

project(RegexInference LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
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

# message(STATUS "=================== Options ===================")

# option(PROFILE_MODE "Show the source code when using Nsight Compute" OFF)
# message(STATUS "PROFILE_MODE is set to: ${PROFILE_MODE}")

# message(STATUS "===============================================")

set(HEADERS
include/bitmask.h 
include/util.hpp 
include/pair.h 
include/types.h
include/guide_table.hpp
include/operations.h
include/top_down.hpp
include/bottom_up.hpp
include/rei.hpp
include/level_partitioner.hpp
include/regex_match.hpp
include/rei_common.hpp
)

set(SOURCES
src/main.cpp
src/util.cpp 
src/top_down.cpp
src/bottom_up.cpp
src/rei.cpp
src/regex_match.cpp
src/guide_table.cpp
src/level_partitioner.cpp
src/operations.cpp
src/rei_common.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_sources(${PROJECT_NAME}
    PRIVATE
        ${SOURCES}
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            ${HEADERS}
)

# cuda section
# set_property(TARGET ${PROJECT_NAME} PROPERTY CUDA_ARCHITECTURES 70;75;80;89)
# target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>)
# target_link_libraries(${PROJECT_NAME} PRIVATE cuda cudart)
# if(PROFILE_MODE)
# 	set(CMAKE_CUDA_FLAGS_RELEASE "${CMAKE_CUDA_FLAGS_RELEASE} --generate-line-info")
# endif()

# Set the startup project for Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})