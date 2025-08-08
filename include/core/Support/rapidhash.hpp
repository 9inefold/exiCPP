//===- Support/rapidhash.hpp ----------------------------------------===//
//
// Copyright (C) 2024-2025 Eightfold
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
/// This file implements the rapidhash API.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/ArrayRef.hpp>
#include <Common/StrRef.hpp>

namespace exi {

u64 rapidHash64(StrRef Data);
u64 rapidHash64(ArrayRef<u8> Data);

u64 rhash_64bits(ArrayRef<u8> Data);

inline u64 rhash_64bits(StrRef Data) {
  return rhash_64bits(ArrayRef(
    Data.bytes_begin(), Data.size()));
}

// TODO: hash-128

} // namespace exi
