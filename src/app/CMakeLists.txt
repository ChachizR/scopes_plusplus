project(ScopesPlusPlusMain)

file(GLOB_RECURSE APP_HEADERS CONFIGURE_DEPENDS "./src/*.hpp")
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "./src/*.cpp")

add_executable(${PROJECT_NAME} ./src/main.cpp ${APP_SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "scopes++"
)

target_include_directories(${PROJECT_NAME} PRIVATE ./src)

target_link_libraries(${PROJECT_NAME} PUBLIC imgui)

find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC OpenGL::GL)

if(MSVC)
    # add precompiled headers for MSVC (pch.hpp & pch.cpp)
    target_compile_options(
        ${PROJECT_NAME} PRIVATE
        /W4
        /WX
    )

    target_compile_options(${PROJECT_NAME} PRIVATE
        /Yupch.hpp # use pch
        /FIpch.hpp # implicit first include
    )

    # 3-b) Tell *just* pch.cpp to *create* the PCH.
    set_source_files_properties(
        src/pch.cpp
        PROPERTIES
        COMPILE_FLAGS
        "/Yc\"pch.hpp\""
    )
endif()
