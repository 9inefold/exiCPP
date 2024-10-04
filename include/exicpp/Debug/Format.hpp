//===- exicpp/Debug/Format.hpp --------------------------------------===//
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

#ifndef EXICPP_DEBUG_FORMAT_HPP
#define EXICPP_DEBUG_FORMAT_HPP

#include "Location.hpp"
#include <fmt/base.h>

#define EXICPP_FMT_LOC() EXICPP_LOCATION(FUNC)
#define EXICPP_LOG_LOC true

#if EXICPP_DEBUG && !defined(NFORMAT)
# define LOG_INTERNAL(...) ::exi::dbg::logInternal<EXICPP_LOG_LOC>(__VA_ARGS__)
#else
# define LOG_INTERNAL(...) ((void)0)
#endif

#define LOG(dbglevel, ...) \
  LOG_INTERNAL(EXICPP_FMT_LOC(), \
    fmt::format(__VA_ARGS__), dbglevel)

#define LOG_INFO(...)   LOG(INFO,     __VA_ARGS__)
#define LOG_WARN(...)   LOG(WARNING,  __VA_ARGS__)
#define LOG_ERROR(...)  LOG(ERROR,    __VA_ARGS__)
#define LOG_FATAL(...)  LOG(FATAL,    __VA_ARGS__)

#define LOG_ERRCODE(err_code) \
  LOG_INTERNAL(EXICPP_FMT_LOC(), \
    GET_ERR_STRING(CErrCode(err_code)), ERROR)

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

} // namespace dbg
} // namespace exi

#endif // EXICPP_DEBUG_FORMAT_HPP
