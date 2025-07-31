//===- exi/Grammar/SchemaFactory.hpp --------------------------------===//
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
/// This file defines the factory used to instantiate schemas.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Box.hpp>
#include <core/Common/unique_function.hpp>
#include <exi/Basic/ErrorCodes.hpp>

namespace exi {

struct ExiOptions;
class ExiDecoder;
class ExiEncoder;
class BodyDecoder;
class BodyEncoder;

namespace decode { class Schema; }
namespace encode { class Schema; }

/// A factory which is specialized for a specific set of options.
class SchemaFactory {
  using decode_t = Box<decode::Schema>;
  using encode_t = Box<encode::Schema>;
  /// Generates a decoder schema for the given input.
  unique_function<decode_t(BodyDecoder*) const> DoDecode;
  /// Generates an encoder schema for the given input.
  unique_function<encode_t(BodyEncoder*) const> DoEncode;
  /// Internal default ctor.
  SchemaFactory() = default;

public:
  ~SchemaFactory();
  /// Creates a factory for builtin schemas.
  ExiResult<SchemaFactory> Builtin(const ExiOptions& Opts);
  
  /// Generates a decoder.
  decode_t Decode(BodyDecoder* BD) EXI_NONNULL(2);
  /// Generates an encoder.
  encode_t Encode(BodyEncoder* ED) EXI_NONNULL(2);
};

} // namespace exi
