//===- exicpp/Debug/Location.hpp ------------------------------------===//
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
//
//  Defines a custom source location type used in logging.
//
//===----------------------------------------------------------------===//

#pragma once

#ifndef EXICPP_DEBUG_BASIC_HPP
#define EXICPP_DEBUG_BASIC_HPP

#include <exicpp/Basic.hpp>
#if EXICPP_CPP(20)
# include <source_location>
#endif

#if EXICPP_HAS_BUILTIN(__builtin_COLUMN)
# define EXICPP_COLUMN __builtin_COLUMN()
# define EXICPP_COLUMN_ 1
#elif __cpp_lib_source_location >= 201907L
# define EXICPP_COLUMN std::source_location::current().column()
# define EXICPP_COLUMN_ 1
#else
# define EXICPP_COLUMN 0
# define EXICPP_COLUMN_ 0
#endif

#define EXICPP_LOCATION_() EXICPP_FUNCTION
#define EXICPP_LOCATION_FUNC() __func__

#define EXICPP_LOCATION(...)        \
::exi::dbg::Location {              \
  __FILE__,                         \
  EXICPP_LOCATION_##__VA_ARGS__(),  \
  int(__LINE__), int(EXICPP_COLUMN) \
}

namespace exi {
namespace dbg {

/// Object representing location information.
struct Location {
  static constexpr bool HasColumn() {
    return !!EXICPP_COLUMN_;
  }
public:
  const char* file = "";
  const char* func = "";
  int line   = -1;
  int column = -1;
};

} // namespace dbg
} // namespace exi

#undef EXICPP_COLUMN_

#endif // EXICPP_DEBUG_BASIC_HPP
