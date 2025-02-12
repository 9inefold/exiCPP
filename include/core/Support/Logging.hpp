//===- Support/Logging.hpp ------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
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
#if EXI_LOGGING
# include <Support/Format.hpp>
# include <Support/raw_ostream.hpp>
#endif

#if EXI_LOGGING

/// Format with a specified debug type.
# define LOG_FORMAT_WITH(LEVEL, TYPE, COLOR, ...)                             \
LOG_WITH_LEVEL_AND_TYPE(LEVEL, TYPE, do {                                     \
  const auto _u_OldCol = dbgs().getColor();                                   \
  dbgs().changeColor(::exi::raw_ostream::COLOR)                               \
    << ::exi::format(__VA_ARGS__) << _u_OldCol;                               \
} while(false))

/// Format with the default debug type.
# define LOG_FORMAT(LEVEL, COLOR, ...)                                        \
 LOG_FORMAT_WITH_TYPE(LEVEL, COLOR, __VA_ARGS__)

#else
# define LOG_FORMAT_WITH(LEVEL, TYPE, COLOR, ...) do { } while(false)
# define LOG_FORMAT(LEVEL, COLOR, ...) do { } while(false)
#endif

/// Formats to `dbgs()` if the log level is at least `ERROR`.
#define LOG_ERROR(...) LOG_FORMAT(ERROR, BRIGHT_RED,     __VA_ARGS__)
/// Formats to `dbgs()` if the log level is at least `WARN`.
#define LOG_WARN(...)  LOG_FORMAT(WARN,  BRIGHT_YELLOW,  __VA_ARGS__)
/// Formats to `dbgs()` if the log level is at least `INFO`.
#define LOG_INFO(...)  LOG_FORMAT(INFO,  BRIGHT_WHITE,   __VA_ARGS__)
/// Formats to `dbgs()` if the log level is `EXTRA` (on `-verbose`).
#define LOG_EXTRA(...) LOG_FORMAT(EXTRA, BRIGHT_WHITE,   __VA_ARGS__)

/// Formats to `dbgs()` if the log level is at least `ERROR`.
#define LOG_ERROR_WITH(TYPE, ...)                                             \
 LOG_FORMAT_WITH(ERROR, TYPE, BRIGHT_RED, __VA_ARGS__)
/// Formats to `dbgs()` if the log level is at least `WARN`.
#define LOG_WARN_WITH(TYPE, ...)                                              \
 LOG_FORMAT_WITH(WARN,  TYPE, BRIGHT_YELLOW, __VA_ARGS__)
/// Formats to `dbgs()` if the log level is at least `INFO`.
#define LOG_INFO_WITH(TYPE, ...)                                              \
 LOG_FORMAT_WITH(INFO,  TYPE, BRIGHT_WHITE, __VA_ARGS__)
/// Formats to `dbgs()` if the log level is `EXTRA` (on `-verbose`).
#define LOG_EXTRA_WITH(TYPE, ...)                                             \
 LOG_FORMAT_WITH(EXTRA, TYPE, BRIGHT_WHITE, __VA_ARGS__)
