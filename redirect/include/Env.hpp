//===- Env.hpp ------------------------------------------------------===//
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
/// This file is an interface for environment variables.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Fundamental.hpp>

namespace re {

struct NameBuf;

inline constexpr const char kEnvTrue[]  = "1;TRUE;YES;ON";
inline constexpr const char kEnvFalse[] = "0;FALSE;NO;OFF";

bool FindEnvironmentVariable(const char* Env, NameBuf& Buf);
bool FindEnvInParamList(const NameBuf& Buf, bool ParamListType);

} // namespace re
