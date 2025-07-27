cmake_minimum_required(VERSION 3.29)                    # defines minimum CMake version
project(ScopesPlusPlus)                                 # defines project name

set(CMAKE_CXX_STANDARD 23)                              # defines C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)                     # requires the specified C++ standard
set(CMAKE_CXX_EXTENSIONS OFF)                           # disables compiler extensions
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)                   # exports compile commands for IDEs    
                                                
set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=modernize-*,readability-*) # enables clang-tidy with specific checks

add_subdirectory(./ext)
add_subdirectory(./src/app)

install(TARGETS ScopesPlusPlusMain)