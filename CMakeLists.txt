cmake_minimum_required(VERSION 3.20)
project(zarch VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -march=native -mtune=native")

find_package(Threads REQUIRED)

set(SOURCES
    src/main.cpp
    src/bitwriter.cpp
    src/bitreader.cpp
    src/huffman.cpp
    src/lz.cpp
    src/filters.cpp
    src/metadata.cpp
    src/io.cpp
    src/thread_pool.cpp
    src/archive.cpp
    src/cli.cpp
)

set(HEADERS
    src/format.hpp
    src/bitwriter.hpp
    src/bitreader.hpp
    src/huffman.hpp
    src/lz.hpp
    src/filters.hpp
    src/metadata.hpp
    src/io.hpp
    src/thread_pool.hpp
    src/archive.hpp
    src/cli.hpp
)

add_executable(zarch ${SOURCES} ${HEADERS})
target_link_libraries(zarch PRIVATE Threads::Threads z stdc++fs)
target_include_directories(zarch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

install(TARGETS zarch RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
