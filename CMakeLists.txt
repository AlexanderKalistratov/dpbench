cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(dpbench
    LANGUAGES CXX
    DESCRIPTION
        "Benchmark suite to evaluate Intel Data Parallel Extensions for Python"
)

find_package(pybind11 CONFIG REQUIRED)
find_package(IntelDPCPP REQUIRED)
find_package(PythonExtensions REQUIRED)
find_package(Python3 COMPONENTS NumPy Development)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(dpbench)

set(ignoreMe "${SKBUILD}")
