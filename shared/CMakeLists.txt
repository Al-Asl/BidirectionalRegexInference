cmake_minimum_required(VERSION 3.24)

set(LIBRARY_NAME shared)

add_library(${LIBRARY_NAME} INTERFACE)

set(SHARED_SOURCES
    src/language_system.cpp
    src/level_partitioner.cpp
    src/regex_match.cpp
    src/rei_common.cpp
    src/utils.cpp
)

set(SHARED_HEADERS
    include/bitmask.h
    include/config.h
    include/language_system.hpp
    include/level_partitioner.hpp
    include/operations.h
    include/pair.h
    include/regex_match.hpp
    include/rei_common.hpp
    include/utils.hpp
    include/types.h
    include/cs_utils.h
)

target_sources(shared
    INTERFACE
        ${SHARED_SOURCES}
        ${SHARED_HEADERS}
)

target_include_directories(${LIBRARY_NAME}
    INTERFACE
        include
)