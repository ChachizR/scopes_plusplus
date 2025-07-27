project(ScopesPlusPlusMain)

file(GLOB_RECURSE APP_HEADERS CONFIGURE_DEPENDS "./src/*.hpp")
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "./src/*.cpp")

add_executable(${PROJECT_NAME} ./src/main.cpp ${APP_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ./src)

target_link_libraries(${PROJECT_NAME} PUBLIC imgui)