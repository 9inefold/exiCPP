//===- exi/Grammar/SchemaGet.cpp ------------------------------------===//
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

#include <exi/Grammar/Schema.hpp>
#include <exi/Decode/BodyDecoder.hpp>
// #include <exi/Encode/BodyEncode.hpp>

namespace exi {

class Schema::Get {
public:
	static StreamReader& Reader(ExiDecoder* D) { return D->Reader; }
	static BumpPtrAllocator& BP(ExiDecoder* D) { return D->BP; }
	static decode::StringTable& Idents(ExiDecoder* D) { return D->Idents; }
};

} // namespace exi
