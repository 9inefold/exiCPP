//===- Support/Filesystem/UniqueID.hpp -------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Relicensed under the Apache License, Version 2.0 (the "License");
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
// This file is cut out of Support/FileSystem.hpp to allow UniqueID to be
// reused without bloating the includes.
//
//===----------------------------------------------------------------===//

#pragma once

#include <Common/DenseMapInfo.hpp>
#include <Common/Fundamental.hpp>
#include <Common/Hashing.hpp>
#include <utility>

namespace exi {
namespace sys::fs {

class UniqueID {
  u64 Device;
  u64 File;

public:
  UniqueID() = default;
  UniqueID(u64 Device, u64 File) : Device(Device), File(File) {}

  bool operator==(const UniqueID &Other) const {
    return Device == Other.Device && File == Other.File;
  }
  bool operator!=(const UniqueID &Other) const { return !(*this == Other); }
  bool operator<(const UniqueID &Other) const {
    /// Don't use std::tie since it bloats the compile time of this header.
    if (Device < Other.Device)
      return true;
    if (Other.Device < Device)
      return false;
    return File < Other.File;
  }

  u64 getDevice() const { return Device; }
  u64 getFile() const { return File; }
};

} // namespace sys::fs

// Support UniqueIDs as DenseMap keys.
template <> struct DenseMapInfo<exi::sys::fs::UniqueID> {
  static inline exi::sys::fs::UniqueID getEmptyKey() {
    auto EmptyKey = DenseMapInfo<std::pair<u64, u64>>::getEmptyKey();
    return {EmptyKey.first, EmptyKey.second};
  }

  static inline exi::sys::fs::UniqueID getTombstoneKey() {
    auto TombstoneKey =
        DenseMapInfo<std::pair<u64, u64>>::getTombstoneKey();
    return {TombstoneKey.first, TombstoneKey.second};
  }

  static hash_code getHashValue(const exi::sys::fs::UniqueID &Tag) {
    return hash_value(std::make_pair(Tag.getDevice(), Tag.getFile()));
  }

  static bool isEqual(const exi::sys::fs::UniqueID &LHS,
                      const exi::sys::fs::UniqueID &RHS) {
    return LHS == RHS;
  }
};

} // namespace exi
