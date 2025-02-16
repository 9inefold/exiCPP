//===- Common/Hashing.hpp -------------------------------------------===//
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
// This file implements the newly proposed standard C++ interfaces for hashing
// arbitrary data and building hash functions for user-defined types. This
// interface was originally proposed in N3333[1] and is currently under review
// for inclusion in a future TR and/or standard.
//
// The primary interfaces provide are comprised of one type and three functions:
//
//  -- 'hash_code' class is an opaque type representing the hash code for some
//     data. It is the intended product of hashing, and can be used to implement
//     hash tables, checksumming, and other common uses of hashes. It is not an
//     integer type (although it can be converted to one) because it is risky
//     to assume much about the internals of a hash_code. In particular, each
//     execution of the program has a high probability of producing a different
//     hash_code for a given input. Thus their values are not stable to save or
//     persist, and should only be used during the execution for the
//     construction of hashing datastructures.
//
//  -- 'hash_value' is a function designed to be overloaded for each
//     user-defined type which wishes to be used within a hashing context. It
//     should be overloaded within the user-defined type's namespace and found
//     via ADL. Overloads for primitive types are provided by this library.
//
//  -- 'hash_combine' and 'hash_combine_range' are functions designed to aid
//      programmers in easily and intuitively combining a set of data into
//      a single hash_code for their object. They should only logically be used
//      within the implementation of a 'hash_value' routine or similar context.
//
// Note that 'hash_combine_range' contains very special logic for hashing
// a contiguous array of integers or pointers. This logic is *extremely* fast,
// on a modern Intel "Gainestown" Xeon (Nehalem uarch) @2.2 GHz, these were
// benchmarked at over 6.5 GiB/s for large keys, and <20 cycles/hash for keys
// under 32-bytes.
//
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <Common/Option.hpp>
#include <Config/ABIBreak.inc>
#include <Support/ErrorHandle.hpp>
#include <Support/SwapByteOrder.hpp>
#include <Support/type_traits.hpp>
#include <algorithm>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>

namespace exi {
template <typename T, typename Enable> struct DenseMapInfo;

/// An opaque object representing a hash code.
///
/// This object represents the result of hashing some entity. It is intended to
/// be used to implement hashtables or other hashing-based data structures.
/// While it wraps and exposes a numeric value, this value should not be
/// trusted to be stable or predictable across processes or executions.
///
/// In order to obtain the hash_code for an object 'x':
/// \code
///   using exi::hash_value;
///   exi::hash_code code = hash_value(x);
/// \endcode
class hash_code {
  usize value;

public:
  /// Default construct a hash_code.
  /// Note that this leaves the value uninitialized.
  hash_code() = default;

  /// Form a hash code directly from a numerical value.
  hash_code(usize value) : value(value) {}

  /// Convert the hash code to its numerical value for use.
  /*explicit*/ operator usize() const { return value; }

  friend bool operator==(const hash_code &lhs, const hash_code &rhs) {
    return lhs.value == rhs.value;
  }
  friend bool operator!=(const hash_code &lhs, const hash_code &rhs) {
    return lhs.value != rhs.value;
  }

  /// Allow a hash_code to be directly run through hash_value.
  friend usize hash_value(const hash_code &code) { return code.value; }
};

/// Compute a hash_code for any integer value.
///
/// Note that this function is intended to compute the same hash_code for
/// a particular value without regard to the pre-promotion type. This is in
/// contrast to hash_combine which may produce different hash_codes for
/// differing argument types even if they would implicit promote to a common
/// type without changing the value.
template <typename T>
std::enable_if_t<is_integral_or_enum_v<T>, hash_code> hash_value(T value);

/// Compute a hash_code for a pointer's address.
///
/// N.B.: This hashes the *address*. Not the value and not the type.
template <typename T> hash_code hash_value(const T *ptr);

/// Compute a hash_code for a pair of objects.
template <typename T, typename U>
hash_code hash_value(const std::pair<T, U> &arg);

/// Compute a hash_code for a tuple.
template <typename... Ts>
hash_code hash_value(const std::tuple<Ts...> &arg);

/// Compute a hash_code for a standard string.
template <typename T>
hash_code hash_value(const std::basic_string<T> &arg);

/// Compute a hash_code for a standard string.
template <typename T> hash_code hash_value(const Option<T> &arg);

// All of the implementation details of actually computing the various hash
// code values are held within this namespace. These routines are included in
// the header file mainly to allow inlining and constant propagation.
namespace hashing {
namespace H {

inline u64 fetch64(const char *p) {
  u64 result;
  std::memcpy(&result, p, sizeof(result));
  if (sys::IsBigEndianHost)
    sys::swapByteOrder(result);
  return result;
}

inline u32 fetch32(const char *p) {
  u32 result;
  std::memcpy(&result, p, sizeof(result));
  if (sys::IsBigEndianHost)
    sys::swapByteOrder(result);
  return result;
}

/// Some primes between 2^63 and 2^64 for various uses.
static constexpr u64 k0 = 0xc3a5c85c97cb3127ULL;
static constexpr u64 k1 = 0xb492b66fbe98f273ULL;
static constexpr u64 k2 = 0x9ae16a3b2f90404fULL;
static constexpr u64 k3 = 0xc949d7c7509e6557ULL;

/// Bitwise right rotate.
/// Normally this will compile to a single instruction, especially if the
/// shift is a manifest constant.
inline u64 rotate(u64 val, usize shift) {
  // Avoid shifting by 64: doing so yields an undefined result.
  return shift == 0 ? val : ((val >> shift) | (val << (64 - shift)));
}

inline u64 shift_mix(u64 val) {
  return val ^ (val >> 47);
}

inline u64 hash_16_bytes(u64 low, u64 high) {
  // Murmur-inspired hashing.
  const u64 kMul = 0x9ddfea08eb382d69ULL;
  u64 a = (low ^ high) * kMul;
  a ^= (a >> 47);
  u64 b = (high ^ a) * kMul;
  b ^= (b >> 47);
  b *= kMul;
  return b;
}

inline u64 hash_1to3_bytes(const char *s, usize len, u64 seed) {
  u8 a = s[0];
  u8 b = s[len >> 1];
  u8 c = s[len - 1];
  u32 y = static_cast<u32>(a) + (static_cast<u32>(b) << 8);
  u32 z = static_cast<u32>(len) + (static_cast<u32>(c) << 2);
  return shift_mix(y * k2 ^ z * k3 ^ seed) * k2;
}

inline u64 hash_4to8_bytes(const char *s, usize len, u64 seed) {
  u64 a = fetch32(s);
  return hash_16_bytes(len + (a << 3), seed ^ fetch32(s + len - 4));
}

inline u64 hash_9to16_bytes(const char *s, usize len, u64 seed) {
  u64 a = fetch64(s);
  u64 b = fetch64(s + len - 8);
  return hash_16_bytes(seed ^ a, rotate(b + len, len)) ^ b;
}

inline u64 hash_17to32_bytes(const char *s, usize len, u64 seed) {
  u64 a = fetch64(s) * k1;
  u64 b = fetch64(s + 8);
  u64 c = fetch64(s + len - 8) * k2;
  u64 d = fetch64(s + len - 16) * k0;
  return hash_16_bytes(exi::rotr<u64>(a - b, 43) +
                           exi::rotr<u64>(c ^ seed, 30) + d,
                       a + exi::rotr<u64>(b ^ k3, 20) - c + len + seed);
}

inline u64 hash_33to64_bytes(const char *s, usize len, u64 seed) {
  u64 z = fetch64(s + 24);
  u64 a = fetch64(s) + (len + fetch64(s + len - 16)) * k0;
  u64 b = exi::rotr<u64>(a + z, 52);
  u64 c = exi::rotr<u64>(a, 37);
  a += fetch64(s + 8);
  c += exi::rotr<u64>(a, 7);
  a += fetch64(s + 16);
  u64 vf = a + z;
  u64 vs = b + exi::rotr<u64>(a, 31) + c;
  a = fetch64(s + 16) + fetch64(s + len - 32);
  z = fetch64(s + len - 8);
  b = exi::rotr<u64>(a + z, 52);
  c = exi::rotr<u64>(a, 37);
  a += fetch64(s + len - 24);
  c += exi::rotr<u64>(a, 7);
  a += fetch64(s + len - 16);
  u64 wf = a + z;
  u64 ws = b + exi::rotr<u64>(a, 31) + c;
  u64 r = shift_mix((vf + ws) * k2 + (wf + vs) * k0);
  return shift_mix((seed ^ (r * k0)) + vs) * k2;
}

inline u64 hash_short(const char *s, usize length, u64 seed) {
  if (length >= 4 && length <= 8)
    return hash_4to8_bytes(s, length, seed);
  if (length > 8 && length <= 16)
    return hash_9to16_bytes(s, length, seed);
  if (length > 16 && length <= 32)
    return hash_17to32_bytes(s, length, seed);
  if (length > 32)
    return hash_33to64_bytes(s, length, seed);
  if (length != 0)
    return hash_1to3_bytes(s, length, seed);

  return k2 ^ seed;
}

/// The intermediate state used during hashing.
/// Currently, the algorithm for computing hash codes is based on CityHash and
/// keeps 56 bytes of arbitrary state.
struct hash_state {
  u64 h0 = 0, h1 = 0, h2 = 0, h3 = 0, h4 = 0, h5 = 0, h6 = 0;

  /// Create a new hash_state structure and initialize it based on the
  /// seed and the first 64-byte chunk.
  /// This effectively performs the initial mix.
  static hash_state create(const char *s, u64 seed) {
    hash_state state = {0,
                        seed,
                        hash_16_bytes(seed, k1),
                        exi::rotr<u64>(seed ^ k1, 49),
                        seed * k1,
                        shift_mix(seed),
                        0};
    state.h6 = hash_16_bytes(state.h4, state.h5);
    state.mix(s);
    return state;
  }

  /// Mix 32-bytes from the input sequence into the 16-bytes of 'a'
  /// and 'b', including whatever is already in 'a' and 'b'.
  static void mix_32_bytes(const char *s, u64 &a, u64 &b) {
    a += fetch64(s);
    u64 c = fetch64(s + 24);
    b = exi::rotr<u64>(b + a + c, 21);
    u64 d = a;
    a += fetch64(s + 8) + fetch64(s + 16);
    b += exi::rotr<u64>(a, 44) + d;
    a += c;
  }

  /// Mix in a 64-byte buffer of data.
  /// We mix all 64 bytes even when the chunk length is smaller, but we
  /// record the actual length.
  void mix(const char *s) {
    h0 = exi::rotr<u64>(h0 + h1 + h3 + fetch64(s + 8), 37) * k1;
    h1 = exi::rotr<u64>(h1 + h4 + fetch64(s + 48), 42) * k1;
    h0 ^= h6;
    h1 += h3 + fetch64(s + 40);
    h2 = exi::rotr<u64>(h2 + h5, 33) * k1;
    h3 = h4 * k1;
    h4 = h0 + h5;
    mix_32_bytes(s, h3, h4);
    h5 = h2 + h6;
    h6 = h1 + fetch64(s + 16);
    mix_32_bytes(s + 32, h5, h6);
    std::swap(h2, h0);
  }

  /// Compute the final 64-bit hash code value based on the current
  /// state and the length of bytes hashed.
  u64 finalize(usize length) {
    return hash_16_bytes(hash_16_bytes(h3, h5) + shift_mix(h1) * k1 + h2,
                         hash_16_bytes(h4, h6) + shift_mix(length) * k1 + h0);
  }
};

/// The seed is non-deterministic per process (address of a function in Support)
/// to prevent having users depend on the particular hash values. On platforms
/// without ASLR, this is still likely non-deterministic per build.
inline u64 get_execution_seed() {
#if EXI_INVARIANTS
  return static_cast<u64>(
      reinterpret_cast<uptr>(&fatal_alloc_error));
#else
  return 0xff51afd7ed558ccdULL;
#endif
}


/// Trait to indicate whether a type's bits can be hashed directly.
///
/// A type trait which is true if we want to combine values for hashing by
/// reading the underlying data. It is false if values of this type must
/// first be passed to hash_value, and the resulting hash_codes combined.
//
// FIXME: We want to replace is_integral_or_enum and is_pointer here with
// a predicate which asserts that comparing the underlying storage of two
// values of the type for equality is equivalent to comparing the two values
// for equality. For all the platforms we care about, this holds for integers
// and pointers, but there are platforms where it doesn't and we would like to
// support user-defined types which happen to satisfy this property.
template <typename T> struct is_hashable_data
  : std::integral_constant<bool, ((is_integral_or_enum_v<T> ||
                                   std::is_pointer<T>::value) &&
                                  64 % sizeof(T) == 0)> {};

// Special case std::pair to detect when both types are viable and when there
// is no alignment-derived padding in the pair. This is a bit of a lie because
// std::pair isn't truly POD, but it's close enough in all reasonable
// implementations for our use case of hashing the underlying data.
template <typename T, typename U> struct is_hashable_data<std::pair<T, U> >
  : std::integral_constant<bool, (is_hashable_data<T>::value &&
                                  is_hashable_data<U>::value &&
                                  (sizeof(T) + sizeof(U)) ==
                                   sizeof(std::pair<T, U>))> {};

/// Helper to get the hashable data representation for a type.
/// This variant is enabled when the type itself can be used.
template <typename T>
std::enable_if_t<is_hashable_data<T>::value, T>
 get_hashable_data(const T &value) {
  return value;
}
/// Helper to get the hashable data representation for a type.
/// This variant is enabled when we must first call hash_value and use the
/// result as our data.
template <typename T>
std::enable_if_t<!is_hashable_data<T>::value, usize>
 get_hashable_data(const T &value) {
  using ::exi::hash_value;
  return hash_value(value);
}

/// Helper to store data from a value into a buffer and advance the
/// pointer into that buffer.
///
/// This routine first checks whether there is enough space in the provided
/// buffer, and if not immediately returns false. If there is space, it
/// copies the underlying bytes of value into the buffer, advances the
/// buffer_ptr past the copied bytes, and returns true.
template <typename T>
bool store_and_advance(char *&buffer_ptr, char *buffer_end, const T& value,
                       usize offset = 0) {
  usize store_size = sizeof(value) - offset;
  if (buffer_ptr + store_size > buffer_end)
    return false;
  const char *value_data = reinterpret_cast<const char *>(&value);
  std::memcpy(buffer_ptr, value_data + offset, store_size);
  buffer_ptr += store_size;
  return true;
}

/// Implement the combining of integral values into a hash_code.
///
/// This overload is selected when the value type of the iterator is
/// integral. Rather than computing a hash_code for each object and then
/// combining them, this (as an optimization) directly combines the integers.
template <typename InputIteratorT>
hash_code hash_combine_range_impl(InputIteratorT first, InputIteratorT last) {
  const u64 seed = get_execution_seed();
  char buffer[64], *buffer_ptr = buffer;
  char *const buffer_end = std::end(buffer);
  while (first != last && store_and_advance(buffer_ptr, buffer_end,
                                            get_hashable_data(*first)))
    ++first;
  if (first == last)
    return hash_short(buffer, buffer_ptr - buffer, seed);
  assert(buffer_ptr == buffer_end);

  hash_state state = state.create(buffer, seed);
  usize length = 64;
  while (first != last) {
    // Fill up the buffer. We don't clear it, which re-mixes the last round
    // when only a partial 64-byte chunk is left.
    buffer_ptr = buffer;
    while (first != last && store_and_advance(buffer_ptr, buffer_end,
                                              get_hashable_data(*first)))
      ++first;

    // Rotate the buffer if we did a partial fill in order to simulate doing
    // a mix of the last 64-bytes. That is how the algorithm works when we
    // have a contiguous byte sequence, and we want to emulate that here.
    std::rotate(buffer, buffer_ptr, buffer_end);

    // Mix this chunk into the current state.
    state.mix(buffer);
    length += buffer_ptr - buffer;
  };

  return state.finalize(length);
}

/// Implement the combining of integral values into a hash_code.
///
/// This overload is selected when the value type of the iterator is integral
/// and when the input iterator is actually a pointer. Rather than computing
/// a hash_code for each object and then combining them, this (as an
/// optimization) directly combines the integers. Also, because the integers
/// are stored in contiguous memory, this routine avoids copying each value
/// and directly reads from the underlying memory.
template <typename ValueT>
std::enable_if_t<is_hashable_data<ValueT>::value, hash_code>
 hash_combine_range_impl(ValueT *first, ValueT *last) {
  const u64 seed = get_execution_seed();
  const char *s_begin = reinterpret_cast<const char *>(first);
  const char *s_end = reinterpret_cast<const char *>(last);
  const usize length = std::distance(s_begin, s_end);
  if (length <= 64)
    return hash_short(s_begin, length, seed);

  const char *s_aligned_end = s_begin + (length & ~63);
  hash_state state = state.create(s_begin, seed);
  s_begin += 64;
  while (s_begin != s_aligned_end) {
    state.mix(s_begin);
    s_begin += 64;
  }
  if (length & 63)
    state.mix(s_end - 64);

  return state.finalize(length);
}

} // namespace H
} // namespace hashing


/// Compute a hash_code for a sequence of values.
///
/// This hashes a sequence of values. It produces the same hash_code as
/// 'hash_combine(a, b, c, ...)', but can run over arbitrary sized sequences
/// and is significantly faster given pointers and types which can be hashed as
/// a sequence of bytes.
template <typename InputIteratorT>
hash_code hash_combine_range(InputIteratorT first, InputIteratorT last) {
  return ::exi::hashing::H::hash_combine_range_impl(first, last);
}


// Implementation details for hash_combine.
namespace hashing {
namespace H {

/// Helper class to manage the recursive combining of hash_combine
/// arguments.
///
/// This class exists to manage the state and various calls involved in the
/// recursive combining of arguments used in hash_combine. It is particularly
/// useful at minimizing the code in the recursive calls to ease the pain
/// caused by a lack of variadic functions.
struct hash_combine_recursive_helper {
  char buffer[64] = {};
  hash_state state;
  const u64 seed;

public:
  /// Construct a recursive hash combining helper.
  ///
  /// This sets up the state for a recursive hash combine, including getting
  /// the seed and buffer setup.
  hash_combine_recursive_helper()
    : seed(get_execution_seed()) {}

  /// Combine one chunk of data into the current in-flight hash.
  ///
  /// This merges one chunk of data into the hash. First it tries to buffer
  /// the data. If the buffer is full, it hashes the buffer into its
  /// hash_state, empties it, and then merges the new chunk in. This also
  /// handles cases where the data straddles the end of the buffer.
  template <typename T>
  char *combine_data(usize &length, char *buffer_ptr, char *buffer_end, T data) {
    if (!store_and_advance(buffer_ptr, buffer_end, data)) {
      // Check for skew which prevents the buffer from being packed, and do
      // a partial store into the buffer to fill it. This is only a concern
      // with the variadic combine because that formation can have varying
      // argument types.
      usize partial_store_size = buffer_end - buffer_ptr;
      std::memcpy(buffer_ptr, &data, partial_store_size);

      // If the store fails, our buffer is full and ready to hash. We have to
      // either initialize the hash state (on the first full buffer) or mix
      // this buffer into the existing hash state. Length tracks the *hashed*
      // length, not the buffered length.
      if (length == 0) {
        state = state.create(buffer, seed);
        length = 64;
      } else {
        // Mix this chunk into the current state and bump length up by 64.
        state.mix(buffer);
        length += 64;
      }
      // Reset the buffer_ptr to the head of the buffer for the next chunk of
      // data.
      buffer_ptr = buffer;

      // Try again to store into the buffer -- this cannot fail as we only
      // store types smaller than the buffer.
      if (!store_and_advance(buffer_ptr, buffer_end, data,
                             partial_store_size)) {
        exi_unreachable("buffer smaller than stored type");
      }
    }
    return buffer_ptr;
  }

  /// Recursive, variadic combining method.
  ///
  /// This function recurses through each argument, combining that argument
  /// into a single hash.
  template <typename T, typename ...Ts>
  hash_code combine(usize length, char *buffer_ptr, char *buffer_end,
                    const T &arg, const Ts &...args) {
    buffer_ptr = combine_data(length, buffer_ptr, buffer_end, get_hashable_data(arg));

    // Recurse to the next argument.
    return combine(length, buffer_ptr, buffer_end, args...);
  }

  /// Base case for recursive, variadic combining.
  ///
  /// The base case when combining arguments recursively is reached when all
  /// arguments have been handled. It flushes the remaining buffer and
  /// constructs a hash_code.
  hash_code combine(usize length, char *buffer_ptr, char *buffer_end) {
    // Check whether the entire set of values fit in the buffer. If so, we'll
    // use the optimized short hashing routine and skip state entirely.
    if (length == 0)
      return hash_short(buffer, buffer_ptr - buffer, seed);

    // Mix the final buffer, rotating it if we did a partial fill in order to
    // simulate doing a mix of the last 64-bytes. That is how the algorithm
    // works when we have a contiguous byte sequence, and we want to emulate
    // that here.
    std::rotate(buffer, buffer_ptr, buffer_end);

    // Mix this chunk into the current state.
    state.mix(buffer);
    length += buffer_ptr - buffer;

    return state.finalize(length);
  }
};

} // namespace H
} // namespace hashing

/// Combine values into a single hash_code.
///
/// This routine accepts a varying number of arguments of any type. It will
/// attempt to combine them into a single hash_code. For user-defined types it
/// attempts to call a \see hash_value overload (via ADL) for the type. For
/// integer and pointer types it directly combines their data into the
/// resulting hash_code.
///
/// The result is suitable for returning from a user's hash_value
/// *implementation* for their user-defined type. Consumers of a type should
/// *not* call this routine, they should instead call 'hash_value'.
template <typename ...Ts> hash_code hash_combine(const Ts &...args) {
  // Recursively hash each argument using a helper class.
  ::exi::hashing::H::hash_combine_recursive_helper helper;
  return helper.combine(0, helper.buffer, helper.buffer + 64, args...);
}

// Implementation details for implementations of hash_value overloads provided
// here.
namespace hashing {
namespace H {

/// Helper to hash the value of a single integer.
///
/// Overloads for smaller integer types are not provided to ensure consistent
/// behavior in the presence of integral promotions. Essentially,
/// "hash_value('4')" and "hash_value('0' + 4)" should be the same.
inline hash_code hash_integer_value(u64 value) {
  // Similar to hash_4to8_bytes but using a seed instead of length.
  const u64 seed = get_execution_seed();
  const char *s = reinterpret_cast<const char *>(&value);
  const u64 a = fetch32(s);
  return hash_16_bytes(seed + (a << 3), fetch32(s + 4));
}

} // namespace H
} // namespace hashing

// Declared and documented above, but defined here so that any of the hashing
// infrastructure is available.
template <typename T>
std::enable_if_t<is_integral_or_enum_v<T>, hash_code> hash_value(T value) {
  return ::exi::hashing::H::hash_integer_value(
      static_cast<u64>(value));
}

// Declared and documented above, but defined here so that any of the hashing
// infrastructure is available.
template <typename T> hash_code hash_value(const T *ptr) {
  return ::exi::hashing::H::hash_integer_value(
    reinterpret_cast<uptr>(ptr));
}

// Declared and documented above, but defined here so that any of the hashing
// infrastructure is available.
template <typename T, typename U>
hash_code hash_value(const std::pair<T, U> &arg) {
  return hash_combine(arg.first, arg.second);
}

template <typename... Ts> hash_code hash_value(const std::tuple<Ts...> &arg) {
  return std::apply([](const auto &...xs) { return hash_combine(xs...); }, arg);
}

// Declared and documented above, but defined here so that any of the hashing
// infrastructure is available.
template <typename T>
hash_code hash_value(const std::basic_string<T> &arg) {
  return hash_combine_range(arg.begin(), arg.end());
}

template <typename T> hash_code hash_value(const Option<T> &arg) {
  return arg ? hash_combine(true, *arg) : hash_value(false);
}

template <> struct DenseMapInfo<hash_code, void> {
  static inline hash_code getEmptyKey() { return hash_code(-1); }
  static inline hash_code getTombstoneKey() { return hash_code(-2); }
  static unsigned getHashValue(hash_code val) {
    return static_cast<unsigned>(usize(val));
  }
  static bool isEqual(hash_code LHS, hash_code RHS) { return LHS == RHS; }
};

} // namespace exi

/// Implement std::hash so that hash_code can be used in STL containers.
namespace std {

template <>
struct hash<exi::hash_code> {
  usize operator()(const exi::hash_code& Val) const {
    return Val;
  }
};

} // namespace std
