//===- exi/Grammar/SchemaFactory.cpp --------------------------------===//
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

#include <exi/Grammar/SchemaFactory.hpp>
#include <core/Common/StringSwitch.hpp>
#include <exi/Basic/ExiOptions.hpp>
//#include <exi/Decode/ChannelDecoder.hpp>
//#include <exi/Decode/OrderedDecoder.hpp>
//#include <exi/Encode/ChannelEncoder.hpp>
#include <exi/Encode/OrderedEncoder.hpp>

using namespace exi;

SchemaFactory::~SchemaFactory() = default;

static bool ValidateBISchemaID(const ExiOptions& Opts) {
  const String* ID = Opts.SchemaID
    .expect("No SchemaID provided!").get();
  if (ID == nullptr || ID->empty())
    // Explicitly empty.
    return true;
  return StringSwitch<bool>(*ID)
    .Cases("__BUILTIN__", "__builtin__", true)
    .Default(false);
}

ExiResult<SchemaFactory> SchemaFactory::Builtin(const ExiOptions& Opts) {
  if (!ValidateOptions(Opts) || !ValidateBISchemaID(Opts))
    return Err(ErrorCode::kInvalidConfig);
  return Err(ExiError::TODO);
}

Box<encode::Schema> SchemaFactory::Encode(BodyEncoder* ED) {
  if EXI_UNLIKELY(!DoEncode)
    return nullptr;
  if EXI_UNLIKELY(!ED)
    return nullptr;
  return DoEncode(ED);
}
