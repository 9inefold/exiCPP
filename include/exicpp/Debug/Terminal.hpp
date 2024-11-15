//===- exicpp/Debug/Terminal.hpp ------------------------------------===//
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

#ifndef EXICPP_DEBUG_TERMINAL_HPP
#define EXICPP_DEBUG_TERMINAL_HPP

#include <exicpp/Features.hpp>
#include <cstdio>

namespace exi {
namespace dbg {

bool isAtty(int fd);
int fileNo(std::FILE* stream);
bool can_use_ansi(bool refresh = false);

} // namespace dbg
} // namespace exi

#endif // EXICPP_DEBUG_TERMINAL_HPP
