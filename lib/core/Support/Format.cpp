//===- Support/Format.cpp -------------------------------------------===//
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
/// This file defines an interface for formatting into streams.
///
//===----------------------------------------------------------------===//

#include <Support/Format.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/MathExtras.hpp>
#include <Support/raw_ostream.hpp>
#include <fmt/format.h>

using namespace exi;

usize IFormatObject::print(char* Buffer, usize BufferSize) const {
  exi_assert(BufferSize, "Invalid buffer size!");
  auto [It, N] = fmt::vformat_to_n(Buffer, BufferSize, Fmt, VArgs);
  // Other implementations yield number of bytes needed, not including the
  // final '\0'.
  if (N >= BufferSize)
    return N + 1;
  // Otherwise N is the length of output (not including the final '\0').
  return N;
}
