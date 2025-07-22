//===- exi/Encode/BodyEncoder.cpp -----------------------------------===//
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
/// This file provides the interface for encoding of the EXI body to a stream.
///
//===----------------------------------------------------------------===//

#include <exi/Encode/BodyEncoder.hpp>
#include <core/Common/MMatch.hpp>
#include <core/Support/Casting.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/Logging.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/Except.hpp>

#define DEBUG_TYPE "ExiEncoder"

using namespace exi;
using namespace exi::encode;

ExiEncoder::ExiEncoder(MaybeBox<ExiOptions>&& Opts) {
  if (auto E = setOptions(std::move(Opts)))
    Throw<argument_error>("Invalid options configuration.");
}

ExiEncoder::~ExiEncoder() {}

//////////////////////////////////////////////////////////////////////////
// Initialization

static ExiError ValidateBoxedOptions(MaybeBox<ExiOptions>& Opts) {
  if (!Opts) {
    LOG_ERROR("Passed null options.");
    return ExiError::kNullptrRef;
  }
  if (Opts.owned())
    return FixupAndValidateOptions(*Opts);
  else
    return ValidateOptions(*Opts);
}

ExiResult<ExiEncoder> ExiEncoder::New(MaybeBox<ExiOptions>&& Opts) {
  if (auto E = ValidateBoxedOptions(Opts)) {
    LOG_ERROR("Invalid options configuration.");
    return Err(E);
  }
  return ExiEncoder(assumed_valid_tag{}, std::move(Opts));
}

ExiError ExiEncoder::setOptions(MaybeBox<ExiOptions>&& Opts) {
  if (Flags.DidHeader || Flags.DidInit) {
    LOG_ERROR("Header has already been written.");
    return ExiError::kInvalidConfig;
  } else if (auto E = ValidateBoxedOptions(Opts)) {
    LOG_ERROR("Invalid options configuration.");
    return E;
  }

  Header.Opts = std::move(Opts);
  Flags.ValidHeader = true;
  return ExiError::OK;
}

//////////////////////////////////////////////////////////////////////////
// BodyEncoder

BodyEncoder::BodyEncoder(ExiOptions& Opts) : Opts(Opts) {}

void BodyEncoder::anchor() {}
