#pragma once

#include "pch.hpp"

namespace scpp {
enum class ErrorCode {
    None = 0,
    FSNotFound,
    FSReadFailed,
    SourceNotFound,
    SourceAlreadyRunning,
};
}