//===- Support/Logging.hpp ------------------------------------------===//
//
// Copyright (C) 2024 Ninefold
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
//     limitations under the License.
//
//===----------------------------------------------------------------===//
///
/// \file
/// This file provides macros for logging at different levels of verbosity.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Support/Debug.hpp>
#include <Support/Format.hpp>

#ifdef EXI_LOG_LINES
# error EXI_LOG_LINES should be defined AFTER including Logging.hpp!
#endif
static constexpr bool EXI_LOG_LINES = false;

namespace exi {

#if EXI_LOGGING

/// Defined in `Debug.cpp`.
void logColoredInDbgWithLevelAndType(
  unsigned Level, const IFormatObject& Fmt, const char* FileAndLine = "")
    EXI_ENABLE_IF(Level <= unsigned(LogLevel::EXTRA), "Invalid logging level!")
    EXI_NONNULL(3);

/// Format with a specified debug type.
/// TODO: Check EXI_PRESERVE_MOST
/// TODO: Add source_location?
# define LOG_FORMAT_WITH(LEVEL, TYPE, ...)                                    \
LOG_WITH_LEVEL_AND_TYPE(LEVEL, TYPE, [&]() EXI_PRESERVE_MOST {                \
  return logColoredInDbgWithLevelAndType(                                     \
    ::exi::LogLevel::LEVEL, ::exi::format(__VA_ARGS__),                       \
    (EXI_LOG_LINES ? __FILE__ ":" STRINGIFY(__LINE__) ": " : ""));            \
}())

/// Format with the default debug type.
# define LOG_FORMAT(LEVEL, ...)                                               \
 LOG_FORMAT_WITH(LEVEL, DEBUG_TYPE, __VA_ARGS__)

#else
# define logColoredInDbgWithLevelAndType(...) ((void)(0))
# define LOG_FORMAT_WITH(LEVEL, TYPE, ...) do { } while(false)
# define LOG_FORMAT(LEVEL, ...) do { } while(false)
#endif

/// Formats to `dbgs()` if the log level is at least `ERROR`.
#define LOG_ERROR(...) LOG_FORMAT_WITH(ERROR, DEBUG_TYPE, __VA_ARGS__)
/// Formats to `dbgs()` if the log level is at least `WARN`.
#define LOG_WARN(...)  LOG_FORMAT_WITH(WARN,  DEBUG_TYPE, __VA_ARGS__)
/// Formats to `dbgs()` if the log level is at least `INFO`.
#define LOG_INFO(...)  LOG_FORMAT_WITH(INFO,  DEBUG_TYPE, __VA_ARGS__)
/// Formats to `dbgs()` if the log level is `EXTRA` (on `-verbose`).
#define LOG_EXTRA(...) LOG_FORMAT_WITH(EXTRA, DEBUG_TYPE, __VA_ARGS__)

/// Formats to `dbgs()` if the log level is at least `ERROR`.
#define LOG_ERROR_WITH(TYPE, ...) LOG_FORMAT_WITH(ERROR, TYPE, __VA_ARGS__)
/// Formats to `dbgs()` if the log level is at least `WARN`.
#define LOG_WARN_WITH(TYPE, ...)  LOG_FORMAT_WITH(WARN,  TYPE, __VA_ARGS__)
/// Formats to `dbgs()` if the log level is at least `INFO`.
#define LOG_INFO_WITH(TYPE, ...)  LOG_FORMAT_WITH(INFO,  TYPE, __VA_ARGS__)
/// Formats to `dbgs()` if the log level is `EXTRA` (on `-verbose`).
#define LOG_EXTRA_WITH(TYPE, ...) LOG_FORMAT_WITH(EXTRA, TYPE, __VA_ARGS__)

} // namespace exi
