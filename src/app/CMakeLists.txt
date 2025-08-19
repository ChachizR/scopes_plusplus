project(ScopesPlusPlusMain)

# ## Main Executable
file(GLOB_RECURSE APP_HEADERS CONFIGURE_DEPENDS "./src/*.hpp")
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "./src/*.cpp")
file(GLOB_RECURSE APP_CL_SOURCES CONFIGURE_DEPENDS "./kernels/*.cl")

add_executable(${PROJECT_NAME} ./src/main.cpp ${APP_SOURCES})
target_sources(${PROJECT_NAME} PRIVATE ${APP_HEADERS})
target_sources(${PROJECT_NAME} PRIVATE ${APP_CL_SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "scopes++"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ./src
)

source_group(
    TREE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PREFIX "Source Files"
    FILES ${APP_SOURCES}
)

source_group(
    TREE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PREFIX "Header Files"
    FILES ${APP_HEADERS}
)

source_group(
    TREE ${CMAKE_CURRENT_SOURCE_DIR}/kernels
    PREFIX "OpenCL Files"
    FILES ${APP_CL_SOURCES}
)

# ## IMGUI
target_link_libraries(${PROJECT_NAME} PRIVATE imgui)

# ## OpenGL
find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)

# ## OpenCL
find_package(OpenCL REQUIRED)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    $<TARGET_PROPERTY:opencl_headers,INTERFACE_INCLUDE_DIRECTORIES>
    ${OpenCL_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${OpenCL_LIBRARIES}
)

# ## GLM
target_include_directories(${PROJECT_NAME}
    PRIVATE
    $<TARGET_PROPERTY:glm,INTERFACE_INCLUDE_DIRECTORIES>
)

# ## NDI
set(NDI_SDK_DIR "${CMAKE_SOURCE_DIR}/ext/ndi")

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${NDI_SDK_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${NDI_SDK_DIR}/Processing.NDI.Lib.Advanced.x64.lib
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${NDI_SDK_DIR}/Processing.NDI.Lib.Advanced.x64.dll
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying NDI library to build directory"
)

# ## Assets
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    COMMENT "Copying assets to build directory"
)

# ## Kernels
add_custom_target(copy_kernels ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/kernels"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/kernels"
    DEPENDS ${CL_KERNELS}
    COMMENT "Copying OpenCL kernels to build directory"
)

add_dependencies(${PROJECT_NAME} copy_kernels)

# ## Compiler Options
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
