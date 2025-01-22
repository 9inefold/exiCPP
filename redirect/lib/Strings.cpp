//===- Strings.cpp --------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
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
/// This file implements utilities for dealing with strings.
/// See https://github.com/8ightfold/headless-compiler/blob/main/hc-rt/xcrt/Generic/String/Utils.hpp
///
//===----------------------------------------------------------------===//

#include <Strings.hpp>
#include <Mem.hpp>

using namespace re;

namespace re {
namespace {

template <usize N> struct _IntN;
template <usize N> struct _UIntN;

template <> struct  _IntN<1UL>  { using type = i8; };
template <> struct  _IntN<2UL>  { using type = i16; };
template <> struct  _IntN<4UL>  { using type = i32; };
template <> struct  _IntN<8UL>  { using type = i64; };

template <> struct _UIntN<1UL>  { using type = u8; };
template <> struct _UIntN<2UL>  { using type = u16; };
template <> struct _UIntN<4UL>  { using type = u32; };
template <> struct _UIntN<8UL>  { using type = u64; };

template <usize N> using intn_t  = typename _IntN<N>::type;
template <usize N> using uintn_t = typename _UIntN<N>::type;

template <typename T> using intty_t  = intn_t<sizeof(T)>;
template <typename T> using uintty_t = uintn_t<sizeof(T)>;

} // namespace `anonymous`
} // namespace re

static inline char to_lower(char C) noexcept {
  static constexpr char Diff = ('a' - 'A');
  return H::is_upper(C) ? char(C + Diff) : C;
}

static constexpr bool kDoUnsafeMultibyteOps = true;

template <typename Ch>
static constexpr usize make_mask() {
  usize out = 0xFF;
  for (int I = 0; I < sizeof(Ch); ++I)
    out = (out << CHAR_BIT) | 0xFF;
  return out;
}

template <typename Int, typename SpacingType = char>
static constexpr Int repeat_byte(Int byte) {
  constexpr usize byteMask = make_mask<SpacingType>();
  constexpr usize byteOff  = bitsizeof_v<SpacingType>;
  Int res = 0;
  byte = byte & byteMask;
  for (usize I = 0; I < sizeof(Int); ++I)
    res = (res << byteOff) | byte;
  return res;
}

template <typename Int, typename Char>
static constexpr bool has_zeros(Int block) {
  static_assert(sizeof(Char) <= 2);
  constexpr usize off = (bitsizeof_v<Char> - 8);
  constexpr Int loBits = repeat_byte<Int, Char>(0x01);
  constexpr Int hiBits = repeat_byte<Int, Char>(0x80) << off;
  Int subtracted = block - loBits;
  return (subtracted & (~block) & hiBits) != 0;
}

//////////////////////////////////////////////////////////////////////////
// [w]strlen:

template <typename Int, typename Char>
static usize xstringlen_wide_read(const Char* Src) {
  constexpr usize alignTo = sizeof(Int);
  const Char* S = Src;
  // Align the pointer to Int.
  for (; uptr(S) % alignTo != 0; ++S) {
    if (*S == Char(L'\0'))
      return usize(S - Src);
  }
  // Read through blocks.
  for (auto* SI = ptr_cast<const Int>(S);
   !has_zeros<Int, Char>(*SI); ++SI) {
    S = ptr_cast<const Char>(SI);
  }
  // Find the null character.
  for (; *S != Char(L'\0'); ++S);
  return usize(S - Src);
}

template <typename Char>
[[maybe_unused]] static usize
 xstringlen_byte_read(const Char* Src) {
  const Char* S = Src;
  for (; *S != Char(L'\0'); ++S);
  return usize(S - Src);
}

template <typename Char>
static usize xstringlen(const Char* Src) {
  static_assert(sizeof(Char) <= 2);
  if constexpr (kDoUnsafeMultibyteOps) {
    using ReadType = re::intn_t<4 * sizeof(Char)>;
    return xstringlen_wide_read<ReadType, Char>(Src);
  } else {
    return xstringlen_byte_read<Char>(Src);
  }
}

static ALWAYS_INLINE usize stringlen(const char* Src) {
  return xstringlen<char>(Src);
}
// [[maybe_unused]] static ALWAYS_INLINE
//  usize wstringlen(const wchar_t* Src) {
//   return xstringlen<wchar_t>(Src);
// }

//////////////////////////////////////////////////////////////////////////
// [w]strnlen:

template <typename Int, typename Char>
static usize xstringnlen_wide_read(const Char* Src, usize n) {
  constexpr usize alignTo = sizeof(Int);
  const Char* S = Src;
  // Align the pointer to Int.
  for (; uptr(S) % alignTo != 0; ++S) {
    if (*S == Char(L'\0') || !(n--))
      return usize(S - Src);
  }
  // Read through blocks.
  for (auto* SI = ptr_cast<const Int>(S);
   !has_zeros<Int, Char>(*SI); ++SI) {
    S = ptr_cast<const Char>(SI);
    if (n < alignTo) break;
    n -= alignTo;
  }
  // Find the null character.
  for (; *S != Char(L'\0'); ++S) {
    if (!(n--)) break;
  }
  return usize(S - Src);
}

template <typename Char>
[[maybe_unused]] static usize
 xstringnlen_byte_read(const Char* Src, usize n) {
  const Char* S = Src;
  for (; *S != Char(L'\0'); ++S) {
    if (!(n--)) break;
  }
  return usize(S - Src);
}

template <typename Char>
static usize xstringnlen(const Char* Src, usize n) {
  static_assert(sizeof(Char) <= 2);
  if constexpr (kDoUnsafeMultibyteOps) {
    using ReadType = re::intn_t<4 * sizeof(Char)>;
    return xstringnlen_wide_read<ReadType, Char>(Src, n);
  } else {
    return xstringnlen_byte_read<Char>(Src, n);
  }
}

static ALWAYS_INLINE usize stringnlen(const char* Src, usize n) {
  return xstringnlen<char>(Src, n);
}
// [[maybe_unused]] static ALWAYS_INLINE
//  usize wstringnlen(const wchar_t* Src, usize n) {
//   return xstringnlen<wchar_t>(Src, n);
// }

//////////////////////////////////////////////////////////////////////////
// [w]strchr:

template <typename Int, typename Char, typename Conv>
static void* xFFC_wide_read(
 const Char* Src, Char C,
 const usize n, Conv&& conv
) {
  using UChar = re::uintty_t<Char>;
  constexpr usize alignTo = sizeof(Int);
  constexpr usize incOff  = sizeof(Int) / sizeof(Char);
  const char CC = conv(C);

  const UChar* S = ptr_cast<const UChar>(Src);
  usize cur = 0;
  // Align the pointer to Int
  for (; uptr(S) % alignTo != 0 && cur < n; ++S, ++cur) {
    if (*S == CC)
      return const_cast<UChar*>(S);
  }
  // Read through blocks.
  const Int C_mask  = repeat_byte<Int, Char>(C);
  const Int CC_mask = repeat_byte<Int, Char>(CC);
  if (C_mask != CC_mask) {
    for (auto* SI = ptr_cast<const Int>(S);
     !has_zeros<Int, Char>((*SI) ^ C_mask)  &&
     !has_zeros<Int, Char>((*SI) ^ CC_mask) && cur < n;
     ++SI, cur += incOff) {
      S = ptr_cast<const UChar>(SI);
    }
  } else {
    for (auto* SI = ptr_cast<const Int>(S);
     !has_zeros<Int, Char>((*SI) ^ C_mask) && cur < n;
     ++SI, cur += incOff) {
      S = ptr_cast<const UChar>(SI);
    }
  }
  // Find match in block.
  for (; conv(*S) != CC && cur < n; ++S, ++cur);

  return (conv(*S) != CC || cur >= n) 
    ? nullptr : const_cast<UChar*>(S);
}

template <typename Char, typename Conv>
[[maybe_unused]] static void*
 xFFC_byte_read(const Char* S, Char C, usize n, Conv&& conv) {
  using UChar = re::uintty_t<Char>;
  const char CC = conv(C);
  for (; n && conv(*S) != CC; --n, ++S);
  return n ? const_cast<Char*>(S) : nullptr;
}

template <typename Char, typename Conv>
static void* xfind_first_char(
 const Char* S, Char C, usize Max, Conv&& conv) {
  static_assert(sizeof(Char) <= 2);
  if constexpr (kDoUnsafeMultibyteOps) {
    using ReadType = re::intn_t<4 * sizeof(Char)>;
    // Check if the overhead of aligning and generating a mask
    // is greater than the overlead of just doing a direct search.
    if (Max > (alignof(ReadType) * 4))
      return xFFC_wide_read<ReadType, Char>(S, C, Max, conv);
  }
  return xFFC_byte_read<Char>(S, C, Max, conv);
}

template <typename Conv>
static char* find_first_char(
 const char* S, char C, usize Max, Conv&& conv) {
  void* const P = xfind_first_char<char>(S, C, Max, conv);
  return static_cast<char*>(P);
}

// [[maybe_unused]] static wchar_t* wfind_first_char(
//  const wchar_t* S, wchar_t C, usize Max) {
//   void* const P = xfind_first_char<wchar_t>(S, C, Max);
//   return static_cast<wchar_t*>(P);
// }

//////////////////////////////////////////////////////////////////////////
// [w]str[n]_cmp:

template <typename Char, typename Cmp>
static constexpr i32 xstrcmp(
 const Char* LHS, const Char* RHS, Cmp&& cmp) {
  using ConvType = const re::uintty_t<Char>*;
  for (; *LHS && !cmp(*LHS, *RHS); ++LHS, ++RHS);
  return cmp(*ConvType(LHS), *ConvType(RHS));
}

template <typename Char, typename Cmp>
static constexpr i32 xstrncmp(
 const Char* LHS, const Char* RHS, usize n, Cmp&& cmp) {
  using ConvType = const re::uintty_t<Char>*;
  if UNLIKELY(n == 0)
    return 0;
  
  for (; n > 1; --n, ++LHS, ++RHS) {
    const Char C = *LHS;
    if (!cmp(C, Char(0)) || cmp(C, *RHS))
      break;
  }
  return cmp(*ConvType(LHS), *ConvType(RHS));
}

//======================================================================//
// re::*
//======================================================================//

usize re::Strlen(const char* Str) {
  return stringlen(Str);
}

usize re::Strnlen(const char* Str, usize Max) {
  return stringnlen(Str, Max);
}

//////////////////////////////////////////////////////////////////////////
// Search

const char* re::Strchr(const char* Str, u8 Val) {
  auto Conv = [] (char C) -> char { return C; };
  const usize Max = re::Strlen(Str);
  return find_first_char(Str, Val, Max, Conv);
}

char* re::Strchr(char* Str, u8 Val) {
  const char* Out = re::Strchr(
    const_cast<const char*>(Str), Val);
  return const_cast<char*>(Out);
}

// Insensitive

const char* re::StrchrInsensitive(const char* Str, u8 Val) {
  if (not H::is_alpha(Val))
    // Do a normal search if the character isn't a letter.
    tail_return re::Strchr(Str, Val);
  auto Conv = [] (char C) -> char { return to_lower(C); };
  const usize Max = re::Strlen(Str);
  return find_first_char(Str, Val, Max, Conv);
}

char* re::StrchrInsensitive(char* Str, u8 Val) {
  if (not H::is_alpha(Val))
    // Do a normal search if the character isn't a letter.
    tail_return re::Strchr(Str, Val);
  const char* Out = re::StrchrInsensitive(
    const_cast<const char*>(Str), Val);
  return const_cast<char*>(Out);
}

//////////////////////////////////////////////////////////////////////////
// Comparison

static constexpr auto NormCmp
  = [](char L, char R) -> i32 { return L - R; };

int re::Strcmp(const char* LHS, const char* RHS) {
  return xstrcmp(LHS, RHS, NormCmp);
}

int re::Strncmp(const char* LHS, const char* RHS, usize Max) {
  return xstrncmp(LHS, RHS, Max, NormCmp);
}

// Insensitive

static constexpr auto InsCmp = [](char L, char R) -> i32 {
  return NormCmp(to_lower(L), to_lower(R));
};

int re::StrcmpInsensitive(const char* LHS, const char* RHS) {
  return xstrcmp(LHS, RHS, InsCmp);
}

int re::StrncmpInsensitive(const char* LHS, const char* RHS, usize Max) {
  return xstrncmp(LHS, RHS, Max, InsCmp);
}
