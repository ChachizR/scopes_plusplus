cmake_minimum_required(VERSION 3.29) # defines minimum CMake version
project(ScopesPlusPlus) # defines project name

set(CMAKE_CXX_STANDARD 23) # defines C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED ON) # requires the specified C++ standard
set(CMAKE_CXX_EXTENSIONS OFF) # disables compiler extensions
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # exports compile commands for IDEs

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
option(SCPP_ENABLE_CLANG_TIDY "Enable clang-tidy for project targets" OFF)

function(scpp_enable_clang_tidy target_name)
    if(SCPP_ENABLE_CLANG_TIDY AND CLANG_TIDY_EXE)
        set_target_properties(
            ${target_name} PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=modernize-*,readability-*"
        )
    endif()
endfunction()

if(DEFINED CMAKE_CXX_CLANG_TIDY)
    message(STATUS "Using externally configured CMAKE_CXX_CLANG_TIDY")
endif()

set(SCPP_ENABLE_NDI_DEFAULT ON)
if(NOT WIN32)
    set(SCPP_ENABLE_NDI_DEFAULT OFF)
endif()

option(SCPP_ENABLE_NDI "Enable NDI input support" ${SCPP_ENABLE_NDI_DEFAULT})

add_subdirectory(./ext) # Add external dependencies

enable_testing()

add_subdirectory(./src/app) # Add main application source

install(TARGETS ScopesPlusPlusMain)
