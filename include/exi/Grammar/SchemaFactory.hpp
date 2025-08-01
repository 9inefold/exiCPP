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

namespace decode {
class Schema;
using factory_result_t = Box<decode::Schema>;
using factory_t = unique_function<factory_result_t(BodyDecoder*) const>;
} // namespace decode

namespace encode {
class Schema;
using factory_result_t = Box<encode::Schema>;
using factory_t = unique_function<factory_result_t(BodyEncoder*) const>;
} // namespace encode

/// A factory which is specialized for a specific set of options.
class [[nodiscard]] SchemaFactory {
  /// Generates a decoder schema for the given input.
  decode::factory_t DoDecode;
  /// Generates an encoder schema for the given input.
  encode::factory_t DoEncode;
  /// Internal default ctor.
  SchemaFactory() = default;

public:
  ~SchemaFactory();
  /// Creates a factory for builtin schemas.
  ExiResult<SchemaFactory> Builtin(const ExiOptions& Opts);
  
  /// Generates a decoder.
  ExiResult<decode::factory_result_t> Decode(BodyDecoder* BD);
  /// Generates an encoder.
  ExiResult<encode::factory_result_t> Encode(BodyEncoder* ED);
};

} // namespace exi
