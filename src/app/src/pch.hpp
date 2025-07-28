#pragma once

#define CL_HPP_TARGET_OPENCL_VERSION 300
#include <CL/opencl.hpp>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

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
#include <cstdint>

#include <chrono>
using namespace std::literals::chrono_literals;

#include <thread>
#include <memory>