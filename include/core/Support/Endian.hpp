//===- Support/Endian.hpp -------------------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Relicensed under the Apache License, Version 2.0 (the "License");
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
//
// This file declares generic functions to read and write endian specific data.
//
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <Common/Fundamental.hpp>
#include <Common/bit.hpp>
#include <Support/SwapByteOrder.hpp>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace exi::support {

// These are named values for common alignments.
enum {aligned = 0, unaligned = 1};

namespace H {

/// ::value is either alignment, or alignof(T) if alignment is 0.
template<class T, int alignment>
struct PickAlignment {
 enum { value = alignment == 0 ? alignof(T) : alignment };
};

} // namespace H

namespace endian {

template <typename value_type>
[[nodiscard]] inline value_type byte_swap(value_type value, endianness endian) {
  if (endian != exi::endianness::native)
    sys::swapByteOrder(value);
  return value;
}

/// Swap the bytes of value to match the given endianness.
template <typename value_type, endianness endian>
[[nodiscard]] inline value_type byte_swap(value_type value) {
  return byte_swap(value, endian);
}

/// Read a value of a particular endianness from memory.
template <typename value_type, usize alignment = unaligned>
[[nodiscard]] inline value_type read(const void *memory, endianness endian) {
  value_type ret;
  
  std::memcpy(static_cast<void *>(&ret),
         EXI_ASSUME_ALIGNED(
             memory, (H::PickAlignment<value_type, alignment>::value)),
         sizeof(value_type));
  return byte_swap<value_type>(ret, endian);
}

template <typename value_type, endianness endian, usize alignment>
[[nodiscard]] inline value_type read(const void *memory) {
  return read<value_type, alignment>(memory, endian);
}

/// Read a value of a particular endianness from a buffer, and increment the
/// buffer past that value.
template <typename value_type, usize alignment = unaligned,
          typename CharT>
[[nodiscard]] inline value_type readNext(const CharT *&memory,
                                         endianness endian) {
  value_type ret = read<value_type, alignment>(memory, endian);
  memory += sizeof(value_type);
  return ret;
}

template <typename value_type, endianness endian,
          usize alignment = unaligned, typename CharT>
[[nodiscard]] inline value_type readNext(const CharT *&memory) {
  return readNext<value_type, alignment, CharT>(memory, endian);
}

/// Write a value to memory with a particular endianness.
template <typename value_type, usize alignment = unaligned>
inline void write(void *memory, value_type value, endianness endian) {
  value = byte_swap<value_type>(value, endian);
  std::memcpy(EXI_ASSUME_ALIGNED(
             memory, (H::PickAlignment<value_type, alignment>::value)),
         &value, sizeof(value_type));
}

template<typename value_type,
         endianness endian,
         usize alignment>
inline void write(void *memory, value_type value) {
  write<value_type, alignment>(memory, value, endian);
}

/// Write a value of a particular endianness, and increment the buffer past that
/// value.
template <typename value_type, usize alignment = unaligned,
          typename CharT>
inline void writeNext(CharT *&memory, value_type value, endianness endian) {
  write(memory, value, endian);
  memory += sizeof(value_type);
}

template <typename value_type, endianness endian,
          usize alignment = unaligned, typename CharT>
inline void writeNext(CharT *&memory, value_type value) {
  writeNext<value_type, alignment, CharT>(memory, value, endian);
}

template <typename value_type>
using make_unsigned_t = std::make_unsigned_t<value_type>;

/// Read a value of a particular endianness from memory, for a location
/// that starts at the given bit offset within the first byte.
template <typename value_type, endianness endian, usize alignment>
[[nodiscard]] inline value_type readAtBitAlignment(const void *memory,
                                                   u64 startBit) {
  assert(startBit < 8);
  if (startBit == 0)
    return read<value_type, endian, alignment>(memory);
  else {
    // Read two values and compose the result from them.
    value_type val[2];
    std::memcpy(&val[0],
           EXI_ASSUME_ALIGNED(
               memory, (H::PickAlignment<value_type, alignment>::value)),
           sizeof(value_type) * 2);
    val[0] = byte_swap<value_type, endian>(val[0]);
    val[1] = byte_swap<value_type, endian>(val[1]);

    // Shift bits from the lower value into place.
    make_unsigned_t<value_type> lowerVal = val[0] >> startBit;
    // Mask off upper bits after right shift in case of signed type.
    make_unsigned_t<value_type> numBitsFirstVal =
        (sizeof(value_type) * 8) - startBit;
    lowerVal &= ((make_unsigned_t<value_type>)1 << numBitsFirstVal) - 1;

    // Get the bits from the upper value.
    make_unsigned_t<value_type> upperVal =
        val[1] & (((make_unsigned_t<value_type>)1 << startBit) - 1);
    // Shift them in to place.
    upperVal <<= numBitsFirstVal;

    return lowerVal | upperVal;
  }
}

/// Write a value to memory with a particular endianness, for a location
/// that starts at the given bit offset within the first byte.
template <typename value_type, endianness endian, usize alignment>
inline void writeAtBitAlignment(void *memory, value_type value,
                                u64 startBit) {
  assert(startBit < 8);
  if (startBit == 0)
    write<value_type, endian, alignment>(memory, value);
  else {
    // Read two values and shift the result into them.
    value_type val[2];
    std::memcpy(&val[0],
           EXI_ASSUME_ALIGNED(
               memory, (H::PickAlignment<value_type, alignment>::value)),
           sizeof(value_type) * 2);
    val[0] = byte_swap<value_type, endian>(val[0]);
    val[1] = byte_swap<value_type, endian>(val[1]);

    // Mask off any existing bits in the upper part of the lower value that
    // we want to replace.
    val[0] &= ((make_unsigned_t<value_type>)1 << startBit) - 1;
    make_unsigned_t<value_type> numBitsFirstVal =
        (sizeof(value_type) * 8) - startBit;
    make_unsigned_t<value_type> lowerVal = value;
    if (startBit > 0) {
      // Mask off the upper bits in the new value that are not going to go into
      // the lower value. This avoids a left shift of a negative value, which
      // is undefined behavior.
      lowerVal &= (((make_unsigned_t<value_type>)1 << numBitsFirstVal) - 1);
      // Now shift the new bits into place
      lowerVal <<= startBit;
    }
    val[0] |= lowerVal;

    // Mask off any existing bits in the lower part of the upper value that
    // we want to replace.
    val[1] &= ~(((make_unsigned_t<value_type>)1 << startBit) - 1);
    // Next shift the bits that go into the upper value into position.
    make_unsigned_t<value_type> upperVal = value >> numBitsFirstVal;
    // Mask off upper bits after right shift in case of signed type.
    upperVal &= ((make_unsigned_t<value_type>)1 << startBit) - 1;
    val[1] |= upperVal;

    // Finally, rewrite values.
    val[0] = byte_swap<value_type, endian>(val[0]);
    val[1] = byte_swap<value_type, endian>(val[1]);
    std::memcpy(EXI_ASSUME_ALIGNED(
               memory, (H::PickAlignment<value_type, alignment>::value)),
           &val[0], sizeof(value_type) * 2);
  }
}

} // namespace endian

namespace H {

template <typename ValueType, endianness Endian, usize Alignment,
          usize ALIGN = PickAlignment<ValueType, Alignment>::value>
struct packed_endian_specific_integral {
  using value_type = ValueType;
  static constexpr endianness endian = Endian;
  static constexpr usize alignment = Alignment;

  packed_endian_specific_integral() = default;

  explicit packed_endian_specific_integral(value_type val) { *this = val; }

  operator value_type() const {
    return endian::read<value_type, endian, alignment>(
      (const void*)Value.buffer);
  }

  void operator=(value_type newValue) {
    endian::write<value_type, endian, alignment>(
      (void*)Value.buffer, newValue);
  }

  packed_endian_specific_integral &operator+=(value_type newValue) {
    *this = *this + newValue;
    return *this;
  }

  packed_endian_specific_integral &operator-=(value_type newValue) {
    *this = *this - newValue;
    return *this;
  }

  packed_endian_specific_integral &operator|=(value_type newValue) {
    *this = *this | newValue;
    return *this;
  }

  packed_endian_specific_integral &operator&=(value_type newValue) {
    *this = *this & newValue;
    return *this;
  }

private:
  struct {
    alignas(ALIGN) char buffer[sizeof(value_type)];
  } Value;

public:
  struct ref {
    explicit ref(void *Ptr) : Ptr(Ptr) {}

    operator value_type() const {
      return endian::read<value_type, endian, alignment>(Ptr);
    }

    void operator=(value_type NewValue) {
      endian::write<value_type, endian, alignment>(Ptr, NewValue);
    }

  private:
    void *Ptr;
  };
};

} // namespace H

using ulittle16_t =
    H::packed_endian_specific_integral<u16, exi::endianness::little,
                                            unaligned>;
using ulittle32_t =
    H::packed_endian_specific_integral<u32, exi::endianness::little,
                                            unaligned>;
using ulittle64_t =
    H::packed_endian_specific_integral<u64, exi::endianness::little,
                                            unaligned>;

using little16_t =
    H::packed_endian_specific_integral<i16, exi::endianness::little,
                                            unaligned>;
using little32_t =
    H::packed_endian_specific_integral<i32, exi::endianness::little,
                                            unaligned>;
using little64_t =
    H::packed_endian_specific_integral<i64, exi::endianness::little,
                                            unaligned>;

using aligned_ulittle16_t =
    H::packed_endian_specific_integral<u16, exi::endianness::little,
                                            aligned>;
using aligned_ulittle32_t =
    H::packed_endian_specific_integral<u32, exi::endianness::little,
                                            aligned>;
using aligned_ulittle64_t =
    H::packed_endian_specific_integral<u64, exi::endianness::little,
                                            aligned>;

using aligned_little16_t =
    H::packed_endian_specific_integral<i16, exi::endianness::little,
                                            aligned>;
using aligned_little32_t =
    H::packed_endian_specific_integral<i32, exi::endianness::little,
                                            aligned>;
using aligned_little64_t =
    H::packed_endian_specific_integral<i64, exi::endianness::little,
                                            aligned>;

using ubig16_t =
    H::packed_endian_specific_integral<u16, exi::endianness::big,
                                            unaligned>;
using ubig32_t =
    H::packed_endian_specific_integral<u32, exi::endianness::big,
                                            unaligned>;
using ubig64_t =
    H::packed_endian_specific_integral<u64, exi::endianness::big,
                                            unaligned>;

using big16_t =
    H::packed_endian_specific_integral<i16, exi::endianness::big,
                                            unaligned>;
using big32_t =
    H::packed_endian_specific_integral<i32, exi::endianness::big,
                                            unaligned>;
using big64_t =
    H::packed_endian_specific_integral<i64, exi::endianness::big,
                                            unaligned>;

using aligned_ubig16_t =
    H::packed_endian_specific_integral<u16, exi::endianness::big,
                                            aligned>;
using aligned_ubig32_t =
    H::packed_endian_specific_integral<u32, exi::endianness::big,
                                            aligned>;
using aligned_ubig64_t =
    H::packed_endian_specific_integral<u64, exi::endianness::big,
                                            aligned>;

using aligned_big16_t =
    H::packed_endian_specific_integral<i16, exi::endianness::big,
                                            aligned>;
using aligned_big32_t =
    H::packed_endian_specific_integral<i32, exi::endianness::big,
                                            aligned>;
using aligned_big64_t =
    H::packed_endian_specific_integral<i64, exi::endianness::big,
                                            aligned>;

using unaligned_uint16_t =
    H::packed_endian_specific_integral<u16, exi::endianness::native,
                                            unaligned>;
using unaligned_uint32_t =
    H::packed_endian_specific_integral<u32, exi::endianness::native,
                                            unaligned>;
using unaligned_uint64_t =
    H::packed_endian_specific_integral<u64, exi::endianness::native,
                                            unaligned>;

using unaligned_int16_t =
    H::packed_endian_specific_integral<i16, exi::endianness::native,
                                            unaligned>;
using unaligned_int32_t =
    H::packed_endian_specific_integral<i32, exi::endianness::native,
                                            unaligned>;
using unaligned_int64_t =
    H::packed_endian_specific_integral<i64, exi::endianness::native,
                                            unaligned>;

template <typename T>
using little_t =
    H::packed_endian_specific_integral<T, exi::endianness::little,
                                            unaligned>;
template <typename T>
using big_t = H::packed_endian_specific_integral<T, exi::endianness::big,
                                                      unaligned>;

template <typename T>
using aligned_little_t =
    H::packed_endian_specific_integral<T, exi::endianness::little,
                                            aligned>;
template <typename T>
using aligned_big_t =
    H::packed_endian_specific_integral<T, exi::endianness::big, aligned>;

namespace endian {

template <typename T, endianness E> [[nodiscard]] inline T read(const void *P) {
  return *(const H::packed_endian_specific_integral<T, E, unaligned> *)P;
}

[[nodiscard]] inline u16 read16(const void *P, endianness E) {
  return read<u16>(P, E);
}
[[nodiscard]] inline u32 read32(const void *P, endianness E) {
  return read<u32>(P, E);
}
[[nodiscard]] inline u64 read64(const void *P, endianness E) {
  return read<u64>(P, E);
}

template <endianness E> [[nodiscard]] inline u16 read16(const void *P) {
  return read<u16, E>(P);
}
template <endianness E> [[nodiscard]] inline u32 read32(const void *P) {
  return read<u32, E>(P);
}
template <endianness E> [[nodiscard]] inline u64 read64(const void *P) {
  return read<u64, E>(P);
}

[[nodiscard]] inline u16 read16le(const void *P) {
  return read16<exi::endianness::little>(P);
}
[[nodiscard]] inline u32 read32le(const void *P) {
  return read32<exi::endianness::little>(P);
}
[[nodiscard]] inline u64 read64le(const void *P) {
  return read64<exi::endianness::little>(P);
}
[[nodiscard]] inline u16 read16be(const void *P) {
  return read16<exi::endianness::big>(P);
}
[[nodiscard]] inline u32 read32be(const void *P) {
  return read32<exi::endianness::big>(P);
}
[[nodiscard]] inline u64 read64be(const void *P) {
  return read64<exi::endianness::big>(P);
}

template <typename T, endianness E> inline void write(void *P, T V) {
  *(H::packed_endian_specific_integral<T, E, unaligned> *)P = V;
}

inline void write16(void *P, u16 V, endianness E) {
  write<u16>(P, V, E);
}
inline void write32(void *P, u32 V, endianness E) {
  write<u32>(P, V, E);
}
inline void write64(void *P, u64 V, endianness E) {
  write<u64>(P, V, E);
}

template <endianness E> inline void write16(void *P, u16 V) {
  write<u16, E>(P, V);
}
template <endianness E> inline void write32(void *P, u32 V) {
  write<u32, E>(P, V);
}
template <endianness E> inline void write64(void *P, u64 V) {
  write<u64, E>(P, V);
}

inline void write16le(void *P, u16 V) {
  write16<exi::endianness::little>(P, V);
}
inline void write32le(void *P, u32 V) {
  write32<exi::endianness::little>(P, V);
}
inline void write64le(void *P, u64 V) {
  write64<exi::endianness::little>(P, V);
}
inline void write16be(void *P, u16 V) {
  write16<exi::endianness::big>(P, V);
}
inline void write32be(void *P, u32 V) {
  write32<exi::endianness::big>(P, V);
}
inline void write64be(void *P, u64 V) {
  write64<exi::endianness::big>(P, V);
}

} // namespace endian

} // namespace exi::support
