//===- Buf.hpp ------------------------------------------------------===//
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
/// This file provides a wrapper for dealing with variadic lists.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Fundamental.hpp>
#include <cstdarg>

namespace re {
namespace H {

class ArgList {
  va_list VList;

public:
  inline ArgList(va_list VList) { va_copy(this->VList, VList); }
  inline ArgList(ArgList &other) { va_copy(this->VList, other.VList); }
  inline ~ArgList() { va_end(this->VList); }

  inline ArgList &operator=(ArgList &rhs) {
    va_copy(VList, rhs.VList);
    return *this;
  }

  template <class T>
  inline T next() { return va_arg(VList, T); }
};

} // namespace H
} // namespace re
