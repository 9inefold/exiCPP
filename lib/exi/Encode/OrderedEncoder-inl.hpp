//===- exi/Encode/OrderedEncoder-inl.hpp -----------------------------===//
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
#include <core/Support/Logging.hpp>
#include <exi/Basic/ExiOptions.hpp>
#include <exi/Basic/ErrorCodes.hpp>

#define DEBUG_TYPE "OrderedEncoder"

namespace exi {

inline bool IsHeaderSufficientlyAligned(BitBuffer Data, AlignKind A) {
  // Needs no aligning.
  if (A == AlignKind::BitPacked)
    return true;
  return Data.bits() == 0;
}

inline bool OrderedEncoder::IsValidHeaderBuffer(BitBuffer Data, AlignKind A) {
  // A minimal header would look like: [10][0][0nnnn].
  // This has no cookie, out-of-band options, and a version below 16.
  // If we have less data than this, it can't possibly be a valid EXI header.
  if (Data.total_bits() < 8)
    return false;
  // If the stream isn't bit-packed, it must be byte-aligned.
  return IsHeaderSufficientlyAligned(Data, A);
}

inline Option<ExiError> OrderedEncoder::shouldEncodeHeader(BitBuffer Data,
                                                           bool KnownValid) const {
  if (DidEncodeHeader) {
    LOG_WARN("Header has already been written.");
    return Some(ExiError::OK);
  } else if (!IsConstructed || !IsStreamInitialized)
    return Some(ErrorCode::kInvalidConfig);
  // Validate our buffer.
  [[maybe_unused]] AlignKind A = Opts.Alignment;
  if (!KnownValid) {
    if EXI_UNLIKELY(!IsValidHeaderBuffer(Data, A))
      return Some(ErrorCode::kInvalidEXIHeader);
  } else
    exi_invariant(IsValidHeaderBuffer(Data, A));
  // We do want to encode!
  return std::nullopt;
}

inline ExiError OrderedEncoder::encodeHeader(BitBuffer Data, bool KnownValid) {
  if (auto OE = shouldEncodeHeader(Data, KnownValid))
    return *OE;
  Writer->writeBitBuffer(Data);
  DidEncodeHeader = true;
  return ExiError::OK;
}

} // namespace exi

#undef DEBUG_TYPE
