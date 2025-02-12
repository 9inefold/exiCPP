//===- Support/LogLevel.hpp -----------------------------------------===//
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
/// This file provides definitions for the various levels of verbosity.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>

namespace exi {

using LogLevelType = int;

struct LogLevel {
  using type = LogLevelType;
  static constexpr type NONE  = 0;
  static constexpr type ERROR = 1;
  static constexpr type WARN  = 2;
  static constexpr type INFO  = 3;
  static constexpr type EXTRA = 4;

  static constexpr type QUIET = NONE;
  static constexpr type VERBOSE = EXTRA;
};

#if EXI_LOGGING
/// Checks if a value `VAL` can be printed under `LEVEL`.
# define hasLogLevelVal(LEVEL, VAL) ((LEVEL) <= (VAL))
/// Checks if a type `TYPE` can be printed under `LEVEL`.
# define hasLogLevel(TYPE, VAL) ((::exi::LogLevel::TYPE) <= (VAL))
#else
# define hasLogLevelVal(LEVEL, VAL) (false)
# define hasLogLevel(TYPE, VAL) (false)
#endif

} // namespace exi
