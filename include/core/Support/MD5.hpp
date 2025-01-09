/* -*- C++ -*-
 * This code is derived from (original license follows):
 *
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 * See md5.c for more information.
 */

#pragma once

#include <Common/StrRef.hpp>
#include <Support/Endian.hpp>
#include <array>
#include <cstdint>

namespace exi {

template <unsigned N> class SmallStr;
template <typename T> class ArrayRef;

class MD5 {
public:
  struct MD5Result : public std::array<u8, 16> {
    SmallStr<32> digest() const;

    u64 low() const {
      // Our MD5 implementation returns the result in little endian, so the low
      // word is first.
      using namespace support;
      return endian::read<u64, exi::endianness::little>(data());
    }

    u64 high() const {
      using namespace support;
      return endian::read<u64, exi::endianness::little>(data() + 8);
    }
    
    std::pair<u64, u64> words() const {
      using namespace support;
      return std::make_pair(high(), low());
    }
  };

  MD5();

  /// Updates the hash for the byte stream provided.
  void update(ArrayRef<u8> Data);

  /// Updates the hash for the StrRef provided.
  void update(StrRef Str);

  /// Finishes off the hash and puts the result in result.
  void final(MD5Result &Result);

  /// Finishes off the hash, and returns the 16-byte hash data.
  MD5Result final();

  /// Finishes off the hash, and returns the 16-byte hash data.
  /// This is suitable for getting the MD5 at any time without invalidating the
  /// internal state, so that more calls can be made into `update`.
  MD5Result result();

  /// Translates the bytes in \p Res to a hex string that is
  /// deposited into \p Str. The result will be of length 32.
  static void stringifyResult(MD5Result &Result, SmallVecImpl<char> &Str);

  /// Computes the hash for a given bytes.
  static MD5Result hash(ArrayRef<u8> Data);

private:
  // Any 32-bit or wider unsigned integer data type will do.
  using MD5_u32plus = u32;

  // Internal State
  struct {
    MD5_u32plus a = 0x67452301;
    MD5_u32plus b = 0xefcdab89;
    MD5_u32plus c = 0x98badcfe;
    MD5_u32plus d = 0x10325476;
    MD5_u32plus hi = 0;
    MD5_u32plus lo = 0;
    u8 buffer[64];
    MD5_u32plus block[16];
  } InternalState;

  const u8 *body(ArrayRef<u8> Data);
};

/// Helper to compute and return lower 64 bits of the given string's MD5 hash.
inline u64 MD5Hash(StrRef Str) {
  using namespace support;

  MD5 Hash;
  Hash.update(Str);
  MD5::MD5Result Result;
  Hash.final(Result);
  // Return the least significant word.
  return Result.low();
}

} // namespace exi
