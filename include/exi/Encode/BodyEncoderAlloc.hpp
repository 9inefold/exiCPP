//===- exi/Encode/BodyEncoderAlloc.hpp ------------------------------===//
//
// Copyright (C) 2025 Eightfold
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
/// This file declares placement new's for OrderedEncoder and ChannelEncoder.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Fundamental.hpp>
#include <core/Support/Allocator.hpp>

namespace exi {
// class ChannelEncoder;
class OrderedEncoder;
using EncoderBumpAllocator = exi::BumpPtrAllocator;
} // namespace exi

// Defined in OrderedEncoder.hpp
void* operator new(usize Bytes, const exi::OrderedEncoder& OE,
                   usize Alignment = 8);
void* operator new[](usize Bytes, const exi::OrderedEncoder& OE,
                     usize Alignment = 8);

// It is good practice to pair new/delete operators.  Also, MSVC gives many
// warnings if a matching delete overload is not declared, even though the
// throw() spec guarantees it will not be implicitly called.
void operator delete(void* Ptr, const exi::OrderedEncoder& OE, usize);
void operator delete[](void* Ptr, const exi::OrderedEncoder& OE, usize);
