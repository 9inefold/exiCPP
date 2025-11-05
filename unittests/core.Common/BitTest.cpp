//===- unittests/Common/BitTest.hpp ---------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://exi.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//

#include <core/Common/bit.hpp>
#include "gtest/gtest.h"
#include <cstdint>
#include <cstdlib>

using namespace exi;

namespace {

TEST(BitTest, BitCast) {
  static const uint8_t kValueU8 = 0x80;
  EXPECT_TRUE(exi::bit_cast<int8_t>(kValueU8) < 0);

  static const uint16_t kValueU16 = 0x8000;
  EXPECT_TRUE(exi::bit_cast<int16_t>(kValueU16) < 0);

  static const float kValueF32 = 5632.34f;
  EXPECT_FLOAT_EQ(kValueF32,
                  exi::bit_cast<float>(exi::bit_cast<uint32_t>(kValueF32)));

  static const double kValueF64 = 87987234.983498;
  EXPECT_DOUBLE_EQ(kValueF64,
                   exi::bit_cast<double>(exi::bit_cast<uint64_t>(kValueF64)));
}

// In these first two tests all of the original_uintx values are truncated
// except for 64. We could avoid this, but there's really no point.

TEST(BitTest, ByteSwapUnsignedRoundTrip) {
  // The point of the bit twiddling of magic is to test with and without bits
  // in every byte.
  uint64_t value = 1;
  for (std::size_t i = 0; i <= sizeof(value); ++i) {
    uint8_t original_uint8 = static_cast<uint8_t>(value);
    EXPECT_EQ(original_uint8, exi::byteswap(exi::byteswap(original_uint8)));

    uint16_t original_uint16 = static_cast<uint16_t>(value);
    EXPECT_EQ(original_uint16, exi::byteswap(exi::byteswap(original_uint16)));

    uint32_t original_uint32 = static_cast<uint32_t>(value);
    EXPECT_EQ(original_uint32, exi::byteswap(exi::byteswap(original_uint32)));

    uint64_t original_uint64 = static_cast<uint64_t>(value);
    EXPECT_EQ(original_uint64, exi::byteswap(exi::byteswap(original_uint64)));

    value = (value << 8) | 0x55; // binary 0101 0101.
  }
}

TEST(BitTest, ByteSwapSignedRoundTrip) {
  // The point of the bit twiddling of magic is to test with and without bits
  // in every byte.
  uint64_t value = 1;
  for (std::size_t i = 0; i <= sizeof(value); ++i) {
    int8_t original_int8 = static_cast<int8_t>(value);
    EXPECT_EQ(original_int8, exi::byteswap(exi::byteswap(original_int8)));

    int16_t original_int16 = static_cast<int16_t>(value);
    EXPECT_EQ(original_int16, exi::byteswap(exi::byteswap(original_int16)));

    int32_t original_int32 = static_cast<int32_t>(value);
    EXPECT_EQ(original_int32, exi::byteswap(exi::byteswap(original_int32)));

    int64_t original_int64 = static_cast<int64_t>(value);
    EXPECT_EQ(original_int64, exi::byteswap(exi::byteswap(original_int64)));

    // Test other sign.
    value *= -1;

    original_int8 = static_cast<int8_t>(value);
    EXPECT_EQ(original_int8, exi::byteswap(exi::byteswap(original_int8)));

    original_int16 = static_cast<int16_t>(value);
    EXPECT_EQ(original_int16, exi::byteswap(exi::byteswap(original_int16)));

    original_int32 = static_cast<int32_t>(value);
    EXPECT_EQ(original_int32, exi::byteswap(exi::byteswap(original_int32)));

    original_int64 = static_cast<int64_t>(value);
    EXPECT_EQ(original_int64, exi::byteswap(exi::byteswap(original_int64)));

    // Return to normal sign and twiddle.
    value *= -1;
    value = (value << 8) | 0x55; // binary 0101 0101.
  }
}

TEST(BitTest, ByteSwap) {
  // Unsigned types.
  EXPECT_EQ(uint8_t(0x11), exi::byteswap(uint8_t(0x11)));
  EXPECT_EQ(uint16_t(0x1122), exi::byteswap(uint16_t(0x2211)));
  EXPECT_EQ(uint32_t(0x11223344), exi::byteswap(uint32_t(0x44332211)));
  EXPECT_EQ(uint64_t(0x1122334455667788ULL),
            exi::byteswap(uint64_t(0x8877665544332211ULL)));

  // Signed types.
  EXPECT_EQ(int8_t(0x11), exi::byteswap(int8_t(0x11)));
  EXPECT_EQ(int16_t(0x1122), exi::byteswap(int16_t(0x2211)));
  EXPECT_EQ(int32_t(0x11223344), exi::byteswap(int32_t(0x44332211)));
  EXPECT_EQ(int64_t(0x1122334455667788LL),
            exi::byteswap(int64_t(0x8877665544332211LL)));
}

TEST(BitTest, HasSingleBit) {
  EXPECT_FALSE(exi::has_single_bit(0U));
  EXPECT_FALSE(exi::has_single_bit(0ULL));

  EXPECT_FALSE(exi::has_single_bit(~0U));
  EXPECT_FALSE(exi::has_single_bit(~0ULL));

  EXPECT_TRUE(exi::has_single_bit(1U));
  EXPECT_TRUE(exi::has_single_bit(1ULL));

  static const int8_t kValueS8 = -128;
  EXPECT_TRUE(exi::has_single_bit(static_cast<uint8_t>(kValueS8)));

  static const int16_t kValueS16 = -32768;
  EXPECT_TRUE(exi::has_single_bit(static_cast<uint16_t>(kValueS16)));
}

TEST(BitTest, BitFloor) {
  EXPECT_EQ(0u, exi::bit_floor(uint8_t(0)));
  EXPECT_EQ(0u, exi::bit_floor(uint16_t(0)));
  EXPECT_EQ(0u, exi::bit_floor(uint32_t(0)));
  EXPECT_EQ(0u, exi::bit_floor(uint64_t(0)));

  EXPECT_EQ(1u, exi::bit_floor(uint8_t(1)));
  EXPECT_EQ(1u, exi::bit_floor(uint16_t(1)));
  EXPECT_EQ(1u, exi::bit_floor(uint32_t(1)));
  EXPECT_EQ(1u, exi::bit_floor(uint64_t(1)));

  EXPECT_EQ(2u, exi::bit_floor(uint8_t(2)));
  EXPECT_EQ(2u, exi::bit_floor(uint16_t(2)));
  EXPECT_EQ(2u, exi::bit_floor(uint32_t(2)));
  EXPECT_EQ(2u, exi::bit_floor(uint64_t(2)));

  EXPECT_EQ(2u, exi::bit_floor(uint8_t(3)));
  EXPECT_EQ(2u, exi::bit_floor(uint16_t(3)));
  EXPECT_EQ(2u, exi::bit_floor(uint32_t(3)));
  EXPECT_EQ(2u, exi::bit_floor(uint64_t(3)));

  EXPECT_EQ(4u, exi::bit_floor(uint8_t(4)));
  EXPECT_EQ(4u, exi::bit_floor(uint16_t(4)));
  EXPECT_EQ(4u, exi::bit_floor(uint32_t(4)));
  EXPECT_EQ(4u, exi::bit_floor(uint64_t(4)));

  EXPECT_EQ(0x40u, exi::bit_floor(uint8_t(0x7f)));
  EXPECT_EQ(0x4000u, exi::bit_floor(uint16_t(0x7fff)));
  EXPECT_EQ(0x40000000u, exi::bit_floor(uint32_t(0x7fffffffu)));
  EXPECT_EQ(0x4000000000000000ull,
            exi::bit_floor(uint64_t(0x7fffffffffffffffull)));

  EXPECT_EQ(0x80u, exi::bit_floor(uint8_t(0x80)));
  EXPECT_EQ(0x8000u, exi::bit_floor(uint16_t(0x8000)));
  EXPECT_EQ(0x80000000u, exi::bit_floor(uint32_t(0x80000000u)));
  EXPECT_EQ(0x8000000000000000ull,
            exi::bit_floor(uint64_t(0x8000000000000000ull)));

  EXPECT_EQ(0x80u, exi::bit_floor(uint8_t(0xff)));
  EXPECT_EQ(0x8000u, exi::bit_floor(uint16_t(0xffff)));
  EXPECT_EQ(0x80000000u, exi::bit_floor(uint32_t(0xffffffffu)));
  EXPECT_EQ(0x8000000000000000ull,
            exi::bit_floor(uint64_t(0xffffffffffffffffull)));
}

TEST(BitTest, BitCeil) {
  EXPECT_EQ(1u, exi::bit_ceil(uint8_t(0)));
  EXPECT_EQ(1u, exi::bit_ceil(uint16_t(0)));
  EXPECT_EQ(1u, exi::bit_ceil(uint32_t(0)));
  EXPECT_EQ(1u, exi::bit_ceil(uint64_t(0)));

  EXPECT_EQ(1u, exi::bit_ceil(uint8_t(1)));
  EXPECT_EQ(1u, exi::bit_ceil(uint16_t(1)));
  EXPECT_EQ(1u, exi::bit_ceil(uint32_t(1)));
  EXPECT_EQ(1u, exi::bit_ceil(uint64_t(1)));

  EXPECT_EQ(2u, exi::bit_ceil(uint8_t(2)));
  EXPECT_EQ(2u, exi::bit_ceil(uint16_t(2)));
  EXPECT_EQ(2u, exi::bit_ceil(uint32_t(2)));
  EXPECT_EQ(2u, exi::bit_ceil(uint64_t(2)));

  EXPECT_EQ(4u, exi::bit_ceil(uint8_t(3)));
  EXPECT_EQ(4u, exi::bit_ceil(uint16_t(3)));
  EXPECT_EQ(4u, exi::bit_ceil(uint32_t(3)));
  EXPECT_EQ(4u, exi::bit_ceil(uint64_t(3)));

  EXPECT_EQ(4u, exi::bit_ceil(uint8_t(4)));
  EXPECT_EQ(4u, exi::bit_ceil(uint16_t(4)));
  EXPECT_EQ(4u, exi::bit_ceil(uint32_t(4)));
  EXPECT_EQ(4u, exi::bit_ceil(uint64_t(4)));

  // The result is the largest representable value for each type.
  EXPECT_EQ(0x80u, exi::bit_ceil(uint8_t(0x7f)));
  EXPECT_EQ(0x8000u, exi::bit_ceil(uint16_t(0x7fff)));
  EXPECT_EQ(0x80000000u, exi::bit_ceil(uint32_t(0x7fffffffu)));
  EXPECT_EQ(0x8000000000000000ull,
            exi::bit_ceil(uint64_t(0x7fffffffffffffffull)));
}

TEST(BitTest, BitWidth) {
  EXPECT_EQ(0, exi::bit_width(uint8_t(0)));
  EXPECT_EQ(0, exi::bit_width(uint16_t(0)));
  EXPECT_EQ(0, exi::bit_width(uint32_t(0)));
  EXPECT_EQ(0, exi::bit_width(uint64_t(0)));

  EXPECT_EQ(1, exi::bit_width(uint8_t(1)));
  EXPECT_EQ(1, exi::bit_width(uint16_t(1)));
  EXPECT_EQ(1, exi::bit_width(uint32_t(1)));
  EXPECT_EQ(1, exi::bit_width(uint64_t(1)));

  EXPECT_EQ(2, exi::bit_width(uint8_t(2)));
  EXPECT_EQ(2, exi::bit_width(uint16_t(2)));
  EXPECT_EQ(2, exi::bit_width(uint32_t(2)));
  EXPECT_EQ(2, exi::bit_width(uint64_t(2)));

  EXPECT_EQ(2, exi::bit_width(uint8_t(3)));
  EXPECT_EQ(2, exi::bit_width(uint16_t(3)));
  EXPECT_EQ(2, exi::bit_width(uint32_t(3)));
  EXPECT_EQ(2, exi::bit_width(uint64_t(3)));

  EXPECT_EQ(3, exi::bit_width(uint8_t(4)));
  EXPECT_EQ(3, exi::bit_width(uint16_t(4)));
  EXPECT_EQ(3, exi::bit_width(uint32_t(4)));
  EXPECT_EQ(3, exi::bit_width(uint64_t(4)));

  EXPECT_EQ(7, exi::bit_width(uint8_t(0x7f)));
  EXPECT_EQ(15, exi::bit_width(uint16_t(0x7fff)));
  EXPECT_EQ(31, exi::bit_width(uint32_t(0x7fffffffu)));
  EXPECT_EQ(63, exi::bit_width(uint64_t(0x7fffffffffffffffull)));

  EXPECT_EQ(8, exi::bit_width(uint8_t(0x80)));
  EXPECT_EQ(16, exi::bit_width(uint16_t(0x8000)));
  EXPECT_EQ(32, exi::bit_width(uint32_t(0x80000000u)));
  EXPECT_EQ(64, exi::bit_width(uint64_t(0x8000000000000000ull)));

  EXPECT_EQ(8, exi::bit_width(uint8_t(0xff)));
  EXPECT_EQ(16, exi::bit_width(uint16_t(0xffff)));
  EXPECT_EQ(32, exi::bit_width(uint32_t(0xffffffffu)));
  EXPECT_EQ(64, exi::bit_width(uint64_t(0xffffffffffffffffull)));
}

TEST(BitTest, CountlZero) {
  uint8_t Z8 = 0;
  uint16_t Z16 = 0;
  uint32_t Z32 = 0;
  uint64_t Z64 = 0;
  EXPECT_EQ(8, exi::countl_zero(Z8));
  EXPECT_EQ(16, exi::countl_zero(Z16));
  EXPECT_EQ(32, exi::countl_zero(Z32));
  EXPECT_EQ(64, exi::countl_zero(Z64));

  uint8_t NZ8 = 42;
  uint16_t NZ16 = 42;
  uint32_t NZ32 = 42;
  uint64_t NZ64 = 42;
  EXPECT_EQ(2, exi::countl_zero(NZ8));
  EXPECT_EQ(10, exi::countl_zero(NZ16));
  EXPECT_EQ(26, exi::countl_zero(NZ32));
  EXPECT_EQ(58, exi::countl_zero(NZ64));

  EXPECT_EQ(8, exi::countl_zero(0x00F000FFu));
  EXPECT_EQ(8, exi::countl_zero(0x00F12345u));
  for (unsigned i = 0; i <= 30; ++i) {
    EXPECT_EQ(int(31 - i), exi::countl_zero(1u << i));
  }

  EXPECT_EQ(8, exi::countl_zero(0x00F1234500F12345ULL));
  EXPECT_EQ(1, exi::countl_zero(1ULL << 62));
  for (unsigned i = 0; i <= 62; ++i) {
    EXPECT_EQ(int(63 - i), exi::countl_zero(1ULL << i));
  }
}

TEST(BitTest, CountrZero) {
  uint8_t Z8 = 0;
  uint16_t Z16 = 0;
  uint32_t Z32 = 0;
  uint64_t Z64 = 0;
  EXPECT_EQ(8, exi::countr_zero(Z8));
  EXPECT_EQ(16, exi::countr_zero(Z16));
  EXPECT_EQ(32, exi::countr_zero(Z32));
  EXPECT_EQ(64, exi::countr_zero(Z64));

  uint8_t NZ8 = 42;
  uint16_t NZ16 = 42;
  uint32_t NZ32 = 42;
  uint64_t NZ64 = 42;
  EXPECT_EQ(1, exi::countr_zero(NZ8));
  EXPECT_EQ(1, exi::countr_zero(NZ16));
  EXPECT_EQ(1, exi::countr_zero(NZ32));
  EXPECT_EQ(1, exi::countr_zero(NZ64));
}

TEST(BitTest, CountlOne) {
  for (int i = 30; i >= 0; --i) {
    // Start with all ones and unset some bit.
    EXPECT_EQ(31 - i, exi::countl_one(0xFFFFFFFF ^ (1 << i)));
  }
  for (int i = 62; i >= 0; --i) {
    // Start with all ones and unset some bit.
    EXPECT_EQ(63 - i, exi::countl_one(0xFFFFFFFFFFFFFFFFULL ^ (1LL << i)));
  }
  for (int i = 30; i >= 0; --i) {
    // Start with all ones and unset some bit.
    EXPECT_EQ(31 - i, exi::countl_one(0xFFFFFFFF ^ (1 << i)));
  }
}

TEST(BitTest, CountrOne) {
  uint8_t AllOnes8 = ~(uint8_t)0;
  uint16_t AllOnes16 = ~(uint16_t)0;
  uint32_t AllOnes32 = ~(uint32_t)0;
  uint64_t AllOnes64 = ~(uint64_t)0;
  EXPECT_EQ(8, exi::countr_one(AllOnes8));
  EXPECT_EQ(16, exi::countr_one(AllOnes16));
  EXPECT_EQ(32, exi::countr_one(AllOnes32));
  EXPECT_EQ(64, exi::countr_one(AllOnes64));

  uint8_t X8 = 6;
  uint16_t X16 = 6;
  uint32_t X32 = 6;
  uint64_t X64 = 6;
  EXPECT_EQ(0, exi::countr_one(X8));
  EXPECT_EQ(0, exi::countr_one(X16));
  EXPECT_EQ(0, exi::countr_one(X32));
  EXPECT_EQ(0, exi::countr_one(X64));

  uint8_t Y8 = 23;
  uint16_t Y16 = 23;
  uint32_t Y32 = 23;
  uint64_t Y64 = 23;
  EXPECT_EQ(3, exi::countr_one(Y8));
  EXPECT_EQ(3, exi::countr_one(Y16));
  EXPECT_EQ(3, exi::countr_one(Y32));
  EXPECT_EQ(3, exi::countr_one(Y64));
}

TEST(BitTest, PopCount) {
  EXPECT_EQ(0, exi::popcount(0U));
  EXPECT_EQ(0, exi::popcount(0ULL));

  EXPECT_EQ(32, exi::popcount(~0U));
  EXPECT_EQ(64, exi::popcount(~0ULL));

  for (int I = 0; I != 32; ++I)
    EXPECT_EQ(1, exi::popcount(1U << I));
}

TEST(BitTest, Rotl) {
  EXPECT_EQ(0x53U, exi::rotl<uint8_t>(0x53, 0));
  EXPECT_EQ(0x4dU, exi::rotl<uint8_t>(0x53, 2));
  EXPECT_EQ(0xa6U, exi::rotl<uint8_t>(0x53, 9));
  EXPECT_EQ(0x9aU, exi::rotl<uint8_t>(0x53, -5));

  EXPECT_EQ(0xabcdU, exi::rotl<uint16_t>(0xabcd, 0));
  EXPECT_EQ(0xf36aU, exi::rotl<uint16_t>(0xabcd, 6));
  EXPECT_EQ(0xaf36U, exi::rotl<uint16_t>(0xabcd, 18));
  EXPECT_EQ(0xf36aU, exi::rotl<uint16_t>(0xabcd, -10));

  EXPECT_EQ(0xdeadbeefU, exi::rotl<uint32_t>(0xdeadbeef, 0));
  EXPECT_EQ(0x7ddfbd5bU, exi::rotl<uint32_t>(0xdeadbeef, 17));
  EXPECT_EQ(0x5b7ddfbdU, exi::rotl<uint32_t>(0xdeadbeef, 41));
  EXPECT_EQ(0xb6fbbf7aU, exi::rotl<uint32_t>(0xdeadbeef, -22));

  EXPECT_EQ(0x12345678deadbeefULL, exi::rotl<uint64_t>(0x12345678deadbeefULL, 0));
  EXPECT_EQ(0xf56df77891a2b3c6ULL, exi::rotl<uint64_t>(0x12345678deadbeefULL, 35));
  EXPECT_EQ(0x8d159e37ab6fbbc4ULL, exi::rotl<uint64_t>(0x12345678deadbeefULL, 70));
  EXPECT_EQ(0xb7dde2468acf1bd5ULL, exi::rotl<uint64_t>(0x12345678deadbeefULL, -19));
}

TEST(BitTest, Rotr) {
  EXPECT_EQ(0x53U, exi::rotr<uint8_t>(0x53, 0));
  EXPECT_EQ(0xd4U, exi::rotr<uint8_t>(0x53, 2));
  EXPECT_EQ(0xa9U, exi::rotr<uint8_t>(0x53, 9));
  EXPECT_EQ(0x6aU, exi::rotr<uint8_t>(0x53, -5));

  EXPECT_EQ(0xabcdU, exi::rotr<uint16_t>(0xabcd, 0));
  EXPECT_EQ(0x36afU, exi::rotr<uint16_t>(0xabcd, 6));
  EXPECT_EQ(0x6af3U, exi::rotr<uint16_t>(0xabcd, 18));
  EXPECT_EQ(0x36afU, exi::rotr<uint16_t>(0xabcd, -10));

  EXPECT_EQ(0xdeadbeefU, exi::rotr<uint32_t>(0xdeadbeef, 0));
  EXPECT_EQ(0xdf77ef56U, exi::rotr<uint32_t>(0xdeadbeef, 17));
  EXPECT_EQ(0x77ef56dfU, exi::rotr<uint32_t>(0xdeadbeef, 41));
  EXPECT_EQ(0xbbf7ab6fU, exi::rotr<uint32_t>(0xdeadbeef, -22));

  EXPECT_EQ(0x12345678deadbeefULL, exi::rotr<uint64_t>(0x12345678deadbeefULL, 0));
  EXPECT_EQ(0x1bd5b7dde2468acfULL, exi::rotr<uint64_t>(0x12345678deadbeefULL, 35));
  EXPECT_EQ(0xbc48d159e37ab6fbULL, exi::rotr<uint64_t>(0x12345678deadbeefULL, 70));
  EXPECT_EQ(0xb3c6f56df77891a2ULL, exi::rotr<uint64_t>(0x12345678deadbeefULL, -19));
}

} // namespace `anonymous`
