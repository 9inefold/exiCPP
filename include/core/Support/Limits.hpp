//===- Support/Limits.hpp -------------------------------------------===//
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
/// This file provides adaptors for numeric_limits.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <limits>

namespace exi {

template <typename T>
EXI_CONST T max_v = std::numeric_limits<T>::max();

template <typename T>
EXI_CONST T min_v = std::numeric_limits<T>::lowest();

template <typename T>
EXI_CONST T smallest_v = std::numeric_limits<T>::min();

} // namespace exi
