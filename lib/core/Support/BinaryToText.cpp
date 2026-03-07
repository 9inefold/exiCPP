//===- Support/BinaryToText.hpp -------------------------------------===//
//
// Copyright (C) 2026 Ninefold
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
/// This file defines utility classes for encoding/decoding strings and uris
/// with base64 and zbase32.
///
//===----------------------------------------------------------------===//

#include <Support/BinaryToText.hpp>
#include <Common/ArrayRef.hpp>
#include <Common/StringExtras.hpp>
#include <Common/SmallVec.hpp>
#include <Support/Error.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/IntCast.hpp>
#include <Support/raw_ostream.hpp>
#include <array>

using namespace exi;

/// Type used for byte lookup tables.
using LUTType = std::array<u8, 256>;

////////////////////////////////////////////////////////////////////////
// base64

// implementation based on:
//  ...

namespace exi::base64 {

} // namespace exi::base64

/// Encodes string to base64, returns the encoded string.
StrRef exi::base64::encode(StrRef Input, SmallVecImpl<char>& Buf) {
  exi_unimplemented("base64::encode unimplemented!");
}

/// Decodes string from base64, returns the decoded string.
Expected<StrRef> exi::base64::decode(StrRef Input, SmallVecImpl<char>& Buf) {
  exi_unimplemented("base64::decode unimplemented!");
}

////////////////////////////////////////////////////////////////////////
// zbase32

// implementation based on:
//  http://philzimmermann.com/docs/human-oriented-base-32-encoding.txt
//  https://github.com/tv42/zbase32/blob/main/zbase32.go

namespace exi::zbase32 {

/// The alphabet for encoded zbase32.
static constexpr exi::StrRef kAlphabet = "ybndrfg8ejkmcpqxot1uwisza345h769";

/// Generates the lookup table.
static constexpr std::array<u8, 256> GenerateLUT() {
  constexpr const char* Alphabet = kAlphabet.data();
  std::array<u8, 256> Out;
  Out.fill(u8(0xFF));
  for (usize I = 0; I < kAlphabet.size(); ++I)
    Out[Alphabet[I]] = u8(I);
  return Out;
}

/// The lookup table used when decoding zbase32.
static constexpr std::array<u8, 256> kDecodeLUT = GenerateLUT();
/// The bits lookup table.
static constexpr usize kBitsLUT[] = {0, 1, 1, 2, 2, 3, 4, 4, 5};

/// EncodedLen returns the maximum length in bytes of the zbase32
/// encoding of an input buffer of length N.
static constexpr usize EncodedLen(usize N) {
  return (N + 4) / 5 * 8;
}

/// DecodedLen returns the maximum length in bytes of the decoded data
/// corresponding to N bytes of zbase32-encoded data.
static constexpr usize DecodedLen(usize N) {
  return (N + 7) / 8 * 5;
}

// CorruptInputError means that the byte at this offset was not a valid
// zbase32 encoding byte.
EXI_NO_INLINE static Error CorruptInputError(usize At, char C) {
  std::string Str;
  raw_string_ostream OS(Str);
  OS << "illegal zbase32 data at input byte " << At << ": ";
  OS.write_escaped(StrRef(&C, 1), /*UseHexEscapes=*/true);
  return exi::createStringError(Str);
}

/// Encode encodes src. It writes at most EncodedLen(len(src)) bytes to
/// dst and returns the number of bytes written.
///
/// Encode is not appropriate for use on individual blocks of a large
/// data stream.
static usize Encode(StrRef src, MutArrayRef<char> dst) {
  usize off = 0;
  for (usize i = 0; !src.empty(); i += 5) {
	  const u8 b0 = u8(src[0]);
	  const u8 b1 = (src.size() > 1) ? u8(src[1]) : u8(0);
	  const unsigned offset = unsigned(i % 8);

	  u8 ch = 0;
	  if (offset < 4) {
		  ch  = (b0 & (31 << (3 - offset))) >> (3 - offset);
		} else {
		  ch  = (b0 & (31 >> (offset - 3))) << (offset - 3);
		  ch |= (b1 & (255 << (11 - offset))) >> (11 - offset);
		}

    exi_invariant(off < dst.size());
	  dst[off++] = kAlphabet[ch];

	  if (offset > 2)
      src = src.drop_front();
	}
  return off;
}

/// Decode decodes zbase32 encoded data from src. It writes at most
/// DecodedLen(len(src)) bytes to dst and returns the number of bytes
/// written.
///
/// If src contains invalid zbase32 data, it will return an Error.
static Expected<usize> Decode(StrRef src, MutArrayRef<char> dst, int bits = -1) {
  const usize olen = src.size();
  usize off = 0;
  while (!src.empty()) {
		// Decode quantum using the zbase32 alphabet
    std::array<u8, 8> dbuf {};

	  int j = 0;
	  for (; j < 8; j++) {
		  if (src.empty())
			  break;
		  const char in = src[0];
		  src = src.drop_front();
		  dbuf[j] = kDecodeLUT[u8(in)];
		  if EXI_UNLIKELY(dbuf[j] == 0xFF) {
        const usize at = olen - src.size() - 1;
			  return CorruptInputError(at, in);
      }
		}

		// 8x 5-bit source blocks, 5 byte destination quantum
	  dst[off+0] = dbuf[0]<<3 | dbuf[1]>>2;
	  dst[off+1] = dbuf[1]<<6 | dbuf[2]<<1 | dbuf[3]>>4;
	  dst[off+2] = dbuf[3]<<4 | dbuf[4]>>1;
	  dst[off+3] = dbuf[4]<<7 | dbuf[5]<<2 | dbuf[6]>>3;
	  dst[off+4] = dbuf[6]<<5 | dbuf[7];

	  off += kBitsLUT[j];
	}
  return off;
}

} // namespace exi::zbase32

/// Encodes string to zbase32, returns the encoded string.
StrRef exi::zbase32::encode(StrRef Input, SmallVecImpl<char>& Buf) {
  const usize N = Input.size();
  Buf.resize(zbase32::EncodedLen(N));
  const usize RLen = zbase32::Encode(Input, Buf);
  return StrRef(Buf.data(), RLen);
}

/// Decodes string from zbase32, returns the decoded string.
Expected<StrRef> exi::zbase32::decode(StrRef Input, SmallVecImpl<char>& Buf) {
  const usize N = Input.size();
  Buf.resize(zbase32::DecodedLen(N));
  Expected<usize> RLenOrErr = zbase32::Decode(Input, Buf);
  if EXI_UNLIKELY(!RLenOrErr)
    return RLenOrErr.takeError();
  return StrRef(Buf.data(), *RLenOrErr);
}
