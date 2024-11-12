//===- utils/Print.hpp ----------------------------------------------===//
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

#pragma once

#include <fmt/format.h>
#include <fmt/color.h>

#define COLOR_PRINT_(col, fstr, ...) \
  fmt::print(fstr, fmt::styled( \
    fmt::format(__VA_ARGS__), fmt::fg(col)))

#define COLOR_PRINT(col, ...)   COLOR_PRINT_(col, "{}", __VA_ARGS__)
#define COLOR_PRINTLN(col, ...) COLOR_PRINT_(col, "{}\n", __VA_ARGS__)

#define PRINT_INFO(...) COLOR_PRINT_( \
  fmt::color::light_blue, "{}\n", __VA_ARGS__)
#define PRINT_WARN(...) COLOR_PRINT_( \
  fmt::color::yellow, "{}\n", "WARNING: " __VA_ARGS__)
#define PRINT_ERR(...)  COLOR_PRINT_( \
  fmt::terminal_color::bright_red, "{}\n", "ERROR: " __VA_ARGS__)
