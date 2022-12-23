cmake_minimum_required(VERSION 3.3)
project(noteshrink-c LANGUAGES CXX)

if (APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
endif()

include_directories(include src/ext)

file(GLOB source_files include/*.h
                       src/*.cpp
                       CMakeLists.txt)
add_executable(noteshrink ${source_files})
