//===- exi/Grammar/Decode/SchemaGet.cpp -----------------------------===//
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
/// This file defines the Schema::Get class, which exposes processor internals.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Support/Casting.hpp>
#include <exi/Grammar/Schema.hpp>
#include <exi/Decode/BodyDecoder.hpp>

namespace exi::decode {

class Schema::Get {
public:
  static BumpPtrAllocator& BP(ExiDecoder* D) { return D->BP; }
  static decode::StringTable& Idents(ExiDecoder* D) { return D->Idents; }

  template <class StrmT>
  static StrmT* Reader(ExiDecoder* D) { return &cast<StrmT>(D->Reader); }
  static OrdReader& Reader(ExiDecoder* D) { return D->Reader; }

  static auto DecodeQName(ExiDecoder* D) { return D->decodeQName(); }
  static auto DecodeNS(ExiDecoder* D) { return D->decodeNS(); }
  static auto DecodeValue(ExiDecoder* D, SmallQName Name) {
    return D->decodeValue(Name);
  }
};

} // namespace exi::decode
