//===- exicpp/Debug/_Format.hpp -------------------------------------===//
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

#ifndef EXICPP_DEBUG__FORMAT_HPP
#define EXICPP_DEBUG__FORMAT_HPP

#include "Location.hpp"
#include <fmt/format.h>
#if EXICPP_ANSI
# include <fmt/color.h>
#endif

#define EXICPP_FMT_LOC() EXICPP_LOCATION(FUNC)

namespace exi {
namespace dbg {

inline constexpr int filenameDepth = 3;

template <bool PrintLoc = true>
void logInternal(
  const dbg::Location& loc,
  const std::string& msg,
  int debugLevel = INFO
);

template <> void logInternal<true>(
  const dbg::Location&, const std::string&, int);
template <> void logInternal<false>(
  const dbg::Location&, const std::string&, int);

/// Flushes `stdout` then aborts.
[[noreturn]] void fatalError();

} // namespace dbg
} // namespace exi

#endif // EXICPP_DEBUG__FORMAT_HPP
