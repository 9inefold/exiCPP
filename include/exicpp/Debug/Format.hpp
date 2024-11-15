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

#include "_Format.hpp"
#include "Terminal.hpp"

#undef EXICPP_LOG_FUNC
#if !NFORMAT
# define EXICPP_LOG_FUNC(...) \
  ::exi::dbg::logInternal<true>(__VA_ARGS__)
#else
# define EXICPP_LOG_FUNC(...) ((void)0)
#endif

#undef LOG_ASSERT
#ifndef NDEBUG
# define LOG_ASSERT(expr) \
  (EXICPP_EXPECT_TRUE(static_cast<bool>(expr)) ? ((void)0) \
    : LOG_FATAL("Assertion failed: {}", #expr))
#else
# define LOG_ASSERT(expr) ((void)0)
#endif

//======================================================================//
// Persistent
//======================================================================//

#ifndef EXICPP_DEBUG_FORMAT_HPP
#define EXICPP_DEBUG_FORMAT_HPP

#if EXICPP_DEBUG
# define LOG_INTERNAL(...) EXICPP_LOG_FUNC(__VA_ARGS__)
#else
# define LOG_INTERNAL(...) ((void)0)
#endif

#define LOG(dbglevel, ...) \
  ((dbglevel >= EXICPP_DEBUG_LEVEL) \
    ? LOG_INTERNAL(EXICPP_FMT_LOC(), \
        fmt::format(__VA_ARGS__), dbglevel) \
    : (void(0)))

#define LOG_INFO(...)   LOG(INFO,     __VA_ARGS__)
#define LOG_WARN(...)   LOG(WARNING,  __VA_ARGS__)
#define LOG_ERROR(...)  LOG(ERROR,    __VA_ARGS__)
#define LOG_FATAL(...)  \
  EXICPP_LOG_FUNC(EXICPP_FMT_LOC(), \
    fmt::format(__VA_ARGS__), FATAL)

#define LOG_ERRCODE(err_code) \
  LOG_INTERNAL(EXICPP_FMT_LOC(), \
    GET_ERR_STRING(CErrCode(err_code)), ERROR)

#endif // EXICPP_DEBUG_FORMAT_HPP
