//===- exi/Encode/HeaderEncoder.cpp ---------------------------------===//
//
// Copyright (C) 2024 Ninefold
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
/// This file implements encoding of the EXI Header to a stream.
///
//===----------------------------------------------------------------===//

#include <exi/Encode/HeaderEncoder.hpp>
#include <core/Support/Casting.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/ExiHeader.hpp>
#include <exi/Basic/NBitInt.hpp>
#include <exi/Decode/UnifyBuffer.hpp>
#include <exi/Encode/BodyEncoder.hpp>

#define DEBUG_TYPE "HeaderEncoder"

using namespace exi;

static ExiError EncodeVersion(const ExiHeader& Header, BitWriter* Strm) {
  // Should always be false.
  exi_invariant(Header.IsPreviewVersion == false);
  Strm->writeBit(/*PreviewVersion=*/false);

  static constexpr u32 VersionChunk = 0b1111;
  u32 Version = Header.ExiVersion - 1;

  // This assume has been tested on GCC and Clang.
  // It WILL remove/unroll the loop, as seen here https://godbolt.org/z/h6hfcPPqh.
  exi_assume(Version < kCurrentExiVersion);
  for (; Version >= VersionChunk; Version -= VersionChunk) {
    Strm->writeBits<4>(VersionChunk);
    Version -= VersionChunk;
  }

  Strm->writeBits<4>(Version);
  return ExiError::OK;
}

static ExiError encodeHeaderImpl(const ExiHeader& Header, BitWriter& Strm) {
  if (ExiError E = exi::FixupAndValidateHeader(Header)) {
    // There was some error with encoding settings.
    LOG_ERROR("error with header settings");
    return E;
  }

  if (Header.HasCookie) {
    Strm.writeAsciiString</*Validate=*/false>("$EXI");
    LOG_EXTRA("header has cookie.");
  }

  Strm.writeBits<2>(0b10);
  Strm.writeBit(Header.HasOptions);
  
  exi_try(EncodeVersion(Header, &Strm));
  LOG_EXTRA("EXI version: {}", Header.ExiVersion);

  if (!Header.HasOptions)
    LOG_EXTRA("options are out-of-band");
  else {
    // TODO: Encode options to file.
    exi_todo("options encode unimplemented");
    return ExiError::TODO;
  }

  if (Header.Opts->Alignment != AlignKind::BitPacked)
    // Skip [Padding Bits].
    Strm.align();

  return ExiError::OK;
}

ExiError exi::encodeHeader(const ExiHeader& Header, OrdWriter& Strm) {
  if EXI_UNLIKELY(Strm.empty()) {
    LOG_ERROR("empty stream cannot be used!");
    return ExiError(ErrorCode::kUnexpectedError);
  }

  if (auto* Bits = dyn_cast<BitWriter>(&Strm)) {
    // Header must be bit packed. If the representation is the same, do nothing.
    return encodeHeaderImpl(Header, *Bits);
  }

  // TODO: Verify this all works
  ByteWriter& Bytes = cast<ByteWriter>(Strm);
  BitWriter Bits(std::move(Bytes));
  
  ExiError Out = encodeHeaderImpl(Header, Bits);
  Strm.emplace<ByteWriter>(std::move(Bits));
  return Out;
}

static ArrayRef<u8> GetU8BufferFromSVec(const SmallVecImpl<char>& V) {
  return ArrayRef(
    reinterpret_cast<const u8*>(V.begin()),
    reinterpret_cast<const u8*>(V.end()));
}

ExiError ExiEncoder::compileHeader(Option<bool> IncludeOptions) {
  if EXI_UNLIKELY(PCH.has_value()) {
    LOG_WARN("Header has already been compiled!");
    return ExiError::OK;
  }

  if (IncludeOptions)
    Header.HasOptions = *IncludeOptions;

  SmallVec<char, 64> Buffer;
  BitWriter Strm(Buffer);
  if (auto E = encodeHeaderImpl(Header, Strm)) {
    LOG_ERROR("Failed to compile header!");
    return E;
  }

  // Record offsets for BitBuffer.
  const usize Bytes = Strm.size();
  const usize Bits = Strm.bitsInStore();
  Strm.flushToWord();
  
  OwningArrayRef Arr(GetU8BufferFromSVec(Buffer));
  PCH = OwningBitBuffer::FromBytesAndBits(std::move(Arr), Bytes, Bits);
  return ExiError::OK;
}
