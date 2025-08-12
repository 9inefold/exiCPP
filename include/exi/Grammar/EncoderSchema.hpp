//===- exi/Grammar/EncoderSchema.hpp --------------------------------===//
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
/// This file defines the base for encoder schemas.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Box.hpp>
#include <core/Common/MaybeBox.hpp>
#include <core/Common/MMatch.hpp>
#include <core/Support/ExtensibleRTTI.hpp>
#include <exi/Basic/EventCodes.hpp>
#include <exi/Encode/Event.hpp>
#include <exi/Grammar/BIState.hpp>
#include <exi/Grammar/SchemaFactory.hpp>
#include <exi/Stream/Writer.hpp>

namespace exi {

struct ExiOptions;
class BodyEncoder;
class ChannelEncoder;
class OrderedEncoder;

namespace encode {

/// The base for all schemas.
class Schema : public RTTIExtends<Schema, RTTIRoot> {
  using EventMatch = MMatch<SimpleEventTerm, SimpleEventTerm>;
protected:
  friend class exi::ChannelEncoder;
  friend class exi::OrderedEncoder;
  template <is_writer_stream> struct Get;

  /// Sets the terminal symbol at the current position.
  virtual ExiError encode(BodyEncoder* BE,
    const BaseEvent& Event, SimpleEventTerm K) = 0;
  
  /// Batches setting the terminal symbols of the same type (if possible).
  virtual ExiError batchEncode(BodyEncoder* BE,
    const void* Arr, usize N, SimpleEventTerm K);
  
  /// Batches setting the terminal symbols of the same type (if possible).
  virtual ExiError batchEncodeRoot(BodyEncoder* BE,
   const void* Arr, usize N, SimpleEventTerm K) {
    return this->batchEncode(BE, Arr, N, K);
  }

public:
  /// Sets the terminal symbol at the current position.
  template <is_encode_event EventT>
  ALWAYS_INLINE ExiError encode(BodyEncoder* BE, const EventT& Event) {
    return this->encode(BE, Event, unmap_event_v<EventT>);
  }
  /// Batches setting the terminal symbols of the same type (if possible).
  template <bool IsRoot = false, is_encode_event EventT>
  ALWAYS_INLINE ExiError batchEncode(BodyEncoder* BE, ArrayRef<EventT> Arr) {
    static_assert(!is_empty_event<EventT>, "Empty events cannot be batched!");
    static constexpr SimpleEventTerm K = unmap_event_v<EventT>;
    using enum SimpleEventTerm;
    static_assert(EventMatch(K).isnt(PI, DT, ER));
    if EXI_NEVER(Arr.size() == 0)
      return ExiError::OK;
    if constexpr (IsRoot)
      return this->batchEncodeRoot(BE, Arr.data(), Arr.size(), K);
    else
      return this->batchEncode(BE, Arr.data(), Arr.size(), K);
  }

  /// Dumps info about the current schema.
  virtual void dump() const {}
private:
  EXI_RTTI_EXTENDS(Schema, RTTIRoot);
  virtual void anchor();
};

/// The builtin (or fallback) schema.
class BuiltinSchema : public RTTIExtends<BuiltinSchema, Schema> {
public:
  using State = BIGrammarState;
  /// @brief Gets a builtin schema factory.
  static factory_t New(const ExiOptions& Opts);
  /// @brief Directly creates a builtin schema.
  static Box<BuiltinSchema> Make(const ExiOptions& Opts, BodyEncoder* BE);
private:
  EXI_RTTI_EXTENDS(BuiltinSchema, Schema);
  void anchor() override;
};

/// A schema which was compiled at runtime.
class DynamicSchema : public RTTIExtends<DynamicSchema, Schema> {
  EXI_RTTI_EXTENDS(DynamicSchema, Schema);
  // TODO: Add Grammar.
  void anchor() override;
};

/// A precompiled schema.
class CompiledSchema : public RTTIExtends<CompiledSchema, Schema> {
  EXI_RTTI_EXTENDS(CompiledSchema, Schema);
  void anchor() override;
};

} // namespace encode
} // namespace exi
