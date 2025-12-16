//===- exi/Encode/OrderedEncoder.cpp --------------------------------===//
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
/// This file implements an exi processor for in-order writers.
///
//===----------------------------------------------------------------===//

#include <exi/Encode/OrderedEncoder.hpp>
#include <core/Common/MMatch.hpp>
#include <core/Support/Casting.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/ExiOptions.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <Encode/OrderedEncoder-inl.hpp>

#define DEBUG_TYPE "OrderedEncoder"

using namespace exi;
using namespace exi::encode;

/// This should never fail, as it gets checked in the header, but check anyways.
EXI_INLINE static void AssertIsOrdered(const ExiOptions& Opts) {
#if EXI_ASSERTS
  using enum AlignKind;
  MMatch Align = mmatch<AlignKind>(Opts.Alignment);
  exi_assert(Align.is(BitPacked, BytePacked), "Invalid alignment type.");
  exi_invariant(!Opts.Compression, "Compression cannot be enabled.");
#endif
}

OrderedEncoder::OrderedEncoder(ExiOptions& Opts, factory_t& F, ExiError* E)
 : BodyEncoder(Opts, EncoderKind::EK_Ordered),
   CurrentSchema(makeSchemaFromThis(F)) {
  ExiError::AsOutParam EAO(E);
  if EXI_UNLIKELY(!CurrentSchema) {
    EAO = ErrorCode::kInvalidConfig;
    return;
  }
  AssertIsOrdered(Opts);
  Strings.setup(Opts);
  IsConstructed = true;
}

ExiError OrderedEncoder::assumeWriterIsEmpty() const {
  if (Writer->size() != 0 || Writer->bitsInStore() != 0) {
    LOG_ERROR("Invalid processor state!");
    return ErrorCode::kInvalidConfig;
  }
  return ExiError::OK;
}

#if 0
ExiError OrderedEncoder::init(raw_ostream& Strm, u32 FlushThreshold) {
  if (IsStreamInitialized) {
    LOG_ERROR("Writer has already been initialized.");
    return ErrorCode::kUnexpectedError;
  }
  initWriter(Strm, FlushThreshold);
  return ExiError::OK;
}
#endif

ExiError OrderedEncoder::init(raw_ostream& Strm) {
  if (IsStreamInitialized) {
    LOG_ERROR("Writer has already been initialized.");
    return ErrorCode::kUnexpectedError;
  }
  initWriter(Strm, OrderedWriter::kFlushThreshold);
  return ExiError::OK;
}

ExiError OrderedEncoder::init(SmallVecImpl<char>& Buf) {
  if (IsStreamInitialized) {
    LOG_ERROR("Writer has already been initialized.");
    return ErrorCode::kUnexpectedError;
  }
  initWriter(Buf);
  return ExiError::OK;
}

ExiError OrderedEncoder::encodeHeader(BitBuffer Data) {
  return this->encodeHeader(Data, false);
}
