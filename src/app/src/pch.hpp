#pragma once

#pragma warning(push,1)
#pragma warning(disable: 26439)

#define CL_HPP_TARGET_OPENCL_VERSION 300
#include <CL/opencl.hpp>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#ifdef _WIN32
#include <windows.h>
#include <wingdi.h>
#elif defined(__APPLE__)
#include <OpenGL/OpenGL.h>
#else // Linux / X11
#include <GL/glx.h>
#endif

#include <GLFW/glfw3.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3native.h>

#include "Processing.NDI.Advanced.h"

#pragma warning(pop)

#include <string>
#include <print>
#include <format>

#include <string_view>
using namespace std::literals::string_view_literals;
#include <span>

#include <vector>
#include <array>

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