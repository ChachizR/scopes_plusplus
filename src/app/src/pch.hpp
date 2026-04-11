#pragma once

#ifdef _MSC_VER
#pragma warning(push,1)
#pragma warning(disable: 26439)
#endif

#include <CL/opencl.hpp>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#define GLFW_INCLUDE_NONE

#ifdef _WIN32
#include <windows.h>
#include <wingdi.h>
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#include <OpenGL/OpenGL.h>
#else // Linux / X11
#include <GL/gl.h>
#include <GL/glx.h>
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <glm/glm.hpp> 

#if defined(SCPP_ENABLE_NDI)
#include "Processing.NDI.Advanced.h"
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <string>
#include <print>
#include <format>

#include <string_view>
using namespace std::literals::string_view_literals;
#include <span>

#include <vector>
#include <array>
#include <map>
#include <unordered_map>

#include <ranges>
#include <algorithm>

#include <expected>
#include <optional>
#include <cstdint>

#include <chrono>
using namespace std::literals::chrono_literals;
using Clock = std::chrono::steady_clock;

#include <thread>
#include <memory>
#include <atomic>

#include <filesystem>
#include <fstream>
#include <cmath>
