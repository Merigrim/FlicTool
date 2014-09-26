cmake_minimum_required(VERSION 2.8)
project(FlicTool)
set(FlicTool_VERSION_MAJOR 1)
set(FlicTool_VERSION_MINOR 0)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if (UNIX OR MINGW)
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11 -g -Wall")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11 -Wall -O0")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_RELEASE}")
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.46.0 COMPONENTS system regex filesystem program_options REQUIRED)

include_directories(${Boost_INCLUDE_DIRS} ${FlicTool_SOURCE_DIR}/include)
set(LIBS ${LIBS} ${Boost_LIBRARIES})

add_subdirectory(src)