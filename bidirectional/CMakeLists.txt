cmake_minimum_required(VERSION 3.24)

project(REI LANGUAGES CXX)

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

# message(STATUS "===============================================")

set(HEADERS
include/top_down.hpp
include/top_down_samplers.hpp
include/bottom_up.hpp
include/rei.hpp
include/analytic_top_down_samplers.hpp
include/generated_stats.hpp
)

set(SOURCES
src/main.cpp
src/top_down.cpp
src/top_down_samplers.cpp
src/bottom_up.cpp
src/rei.cpp
src/analytic_top_down_samplers.cpp
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

target_compile_definitions(${PROJECT_NAME}
    PRIVATE 
    SOLUTIONS_NUM=1
)

target_link_libraries(${PROJECT_NAME} PRIVATE shared)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /Zc:__cplusplus)
endif()

# Set the startup project for Visual Studio
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})