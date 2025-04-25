//===- exi/Grammar/Schema.hpp ---------------------------------------===//
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
/// This file defines the base for schemas.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Box.hpp>
#include <core/Common/MaybeBox.hpp>
#include <core/Support/ExtensibleRTTI.hpp>
#include <exi/Basic/EventCodes.hpp>
#include <exi/Stream/StreamVariant.hpp>

namespace exi {

struct ExiOptions;
class ExiDecoder;
class ExiEncoder;

/// The base for all schemas.
class Schema : public RTTIExtends<Schema, RTTIRoot> {
  friend class ExiDecoder;
  friend class ExiEncoder;
public:
  static const char ID;
  /// Gets the terminal symbol at the current position.
  [[nodiscard]] virtual EventUID decode(ExiDecoder* D) = 0;
  /// TODO: encode(StreamWriter& Strm, EventCode);
  virtual void dump() const {}
protected:
  class Get;
private:
  virtual void anchor();
};

/// The builtin (or fallback) schema.
class BuiltinSchema : public RTTIExtends<BuiltinSchema, Schema> {
protected:
  /// Possible Grammar states for schemaless.
  enum class Grammar {
    Document,
    DocContent,
    DocEnd,
    StartTagContent,
    ElementContent,
    Fragment,
    Last = ElementContent
  };
  
public:
  static const char ID;
  /// @brief Gets a builtin schema.
  [[nodiscard]] static Box<BuiltinSchema> New(const ExiOptions& Opts);
private:
  virtual void anchor();
};

/// A schema which was compiled at runtime.
class DynamicSchema : public RTTIExtends<DynamicSchema, Schema> {
  // TODO: Add Grammar.
public:
  static const char ID;
private:
  virtual void anchor();
};

/// A precompiled schema.
class CompiledSchema : public RTTIExtends<CompiledSchema, Schema> {
public:
  static const char ID;
private:
  virtual void anchor();
};

} // namespace exi
