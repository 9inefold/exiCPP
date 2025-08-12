//===- exi/Grammar/Encode/Schema.cpp --------------------------------===//
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
/// This file defines some functions used in all schema implementations.
///
//===----------------------------------------------------------------===//

#include <exi/Grammar/EncoderSchema.hpp>
#include <core/Common/EnumArray.hpp>
#include <core/Support/Alignment.hpp>
#include <exi/Encode/D/EventMappings.mac>

using namespace exi;
using namespace exi::encode;

consteval EnumArray<usize, SimpleEventTerm> GenerateEncodeSizeofMappings() {
  EnumArray<usize, SimpleEventTerm> A;
# define EVENT_SIZEOF(FROM, TO)                                               \
    A[SimpleEventTerm::FROM] = sizeof(TO);
  EXI_ENCODE_EVENT_MAPPINGS(EVENT_SIZEOF)
  return A;
}
consteval EnumArray<Align, SimpleEventTerm> GenerateEncodeAlignofMappings() {
  EnumArray<Align, SimpleEventTerm> A;
# define EVENT_ALIGNOF(FROM, TO)                                              \
    A[SimpleEventTerm::FROM] = Align::Of<TO>();
  EXI_ENCODE_EVENT_MAPPINGS(EVENT_ALIGNOF)
  return A;
}
/// Maps from `SimpleEventTerm` to `sizeof(TypedEvent)`.
static constexpr auto EncodeSizeofMappings = GenerateEncodeSizeofMappings();
/// Maps from `SimpleEventTerm` to `Align::Of<TypedEvent>()`.
static constexpr auto EncodeAlignofMappings = GenerateEncodeAlignofMappings();

static bool IsEventDataAligned(const void* Raw, SimpleEventTerm K) {
  return isAddrAligned(EncodeAlignofMappings[K], Raw);
}
ALWAYS_INLINE static const BaseEvent& GetBaseEvent(const void* Raw) {
  return *static_cast<const BaseEvent*>(Raw);
}

ExiError Schema::batchEncode(BodyEncoder* BE,
                             const void* Arr, usize N, SimpleEventTerm K) {
  // TODO: Profile this. It may be faster to dispatch since it's a single location.
  exi_assert(N != 0, "Empty arrays should be culled in the templated encode! "
                     "Dispatching is slow and we want to avoid it if possible.");
  exi_invariant(IsEventDataAligned(Arr, K),
                "encodeBatch passed unaligned data, possibly invalid type?");
  auto* P = static_cast<const u8*>(Arr);
  for (usize Ix = 0; Ix < N - 1; ++Ix) {
    exi_try(this->encode(BE, GetBaseEvent(P), K));
    P += EncodeSizeofMappings[K];
  }
  return this->encode(BE, GetBaseEvent(P), K);
}

void Schema::anchor() {}
void BuiltinSchema::anchor() {}
void DynamicSchema::anchor() {}
void CompiledSchema::anchor() {}

char Schema::ID = 0;
char BuiltinSchema::ID = 0;
char DynamicSchema::ID = 0;
char CompiledSchema::ID = 0;
