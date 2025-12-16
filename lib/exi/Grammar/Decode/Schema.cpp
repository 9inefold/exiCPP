//===- exi/Grammar/Decode/Schema.cpp --------------------------------===//
//
// Copyright (C) 2025 Ninefold
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
/// This file defines some functions used in all schema implementations.
///
//===----------------------------------------------------------------===//

#include <exi/Grammar/DecoderSchema.hpp>

using namespace exi;
using namespace exi::decode;

void Schema::anchor() {}
void BuiltinSchema::anchor() {}
void DynamicSchema::anchor() {}
void CompiledSchema::anchor() {}

char Schema::ID = 0;
char BuiltinSchema::ID = 0;
char DynamicSchema::ID = 0;
char CompiledSchema::ID = 0;
