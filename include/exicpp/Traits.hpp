//===- exicpp/Traits.hpp --------------------------------------------===//
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
//
//  Provides traits if not found in the current version.
//
//===----------------------------------------------------------------===//

#pragma once

#ifndef EXIP_TRAITS_HPP
#define EXIP_TRAITS_HPP

#include "Basic.hpp"
#include <string>
#include <type_traits>

#if EXICPP_CONCEPTS
# include <concepts>
#endif

namespace exi {
  template <bool B>
  using bool_constant = std::integral_constant<bool, B>;
} // namespace exi

//======================================================================//
// remove_cvref[_t]
//======================================================================//

namespace exi {
#if __cpp_lib_remove_cvref >= 201711L

using std::remove_cvref;
using std::remove_cvref_t;

#elif EXICPP_HAS_BUILTIN(__remove_cvref)

# if EXICPP_COMPILER(GCC)
/// A fix for strange errors when using using builtins directly on GCC.
template <typename T>
struct RemoveCVRefGCC {
  using type = __remove_cvref(T);
};

namespace H {
  template <typename T>
  using RemoveCVRefType EXICPP_NODEBUG =
    typename RemoveCVRefGCC<T>::type;
} // namespace H
# else
namespace H {
  template <typename T>
  using RemoveCVRefType EXICPP_NODEBUG = __remove_cvref(T);
} // namespace H
# endif

template <typename T>
struct remove_cvref {
  using type = __remove_cvref(T);
};

template <typename T>
using remove_cvref_t = RemoveCVRefType<T>;

#else

template <typename T>
struct remove_cvref {
  using type = typename std::remove_cv<
    typename std::remove_reference<T>::type
  >::type;
};

template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

#endif
} // namespace exi

//======================================================================//
// is_stringlike[_v]
//======================================================================//

namespace exi {

#if EXICPP_CPP(20)

template <typename T>
concept IIsCharType =
  std::same_as<T, char>
  || std::same_as<T, wchar_t>
  || std::same_as<T, char8_t>
  || std::same_as<T, char16_t>
  || std::same_as<T, char32_t>;

template <typename T>
concept IsCharType = IIsCharType<std::remove_cv_t<T>>;

#else

template <typename>
struct IIsCharType : std::false_type {};

template <>
struct IIsCharType<char> : std::true_type {};

template <>
struct IIsCharType<wchar_t> : std::true_type {};

template <typename T>
inline constexpr bool IsCharType = IIsCharType<
  typename std::remove_cv<T>::type
>::value;

#endif

namespace H {
  template <typename T>
  using IsCharTypeC = bool_constant<IsCharType<T>>;
} // namespace H

template <typename T>
struct IsStringlike : std::false_type {};

template <typename T>
struct IsStringlike<T*> : H::IsCharTypeC<T> {};

template <typename T, std::size_t N>
struct IsStringlike<T[N]> : H::IsCharTypeC<T> {};

template <typename T, std::size_t N>
struct IsStringlike<T(&)[N]> : H::IsCharTypeC<T> {};

template <typename T>
struct IsStringlike<T[]> : H::IsCharTypeC<T> {};

template <typename T>
struct is_stringlike : IsStringlike<remove_cvref_t<T>> {};

template <typename T>
inline constexpr bool is_stringlike_v =
  IsStringlike<remove_cvref_t<T>>::value;

} // namespace exi

//======================================================================//
// strsize/strdata
//======================================================================//

namespace exi {

namespace H {
#if EXICPP_HAS_BUILTIN(__builtin_strlen)
  EXICPP_NODEBUG ALWAYS_INLINE constexpr
   std::size_t istrsize(const char* str) {
    return __builtin_strlen(str);
  }
#endif
#if EXICPP_HAS_BUILTIN(__builtin_wcslen)
  EXICPP_NODEBUG ALWAYS_INLINE constexpr
   std::size_t istrsize(const wchar_t* str) {
    return __builtin_wcslen(str);
  }
#endif
  template <typename T>
  EXICPP_NODEBUG ALWAYS_INLINE EXICPP_CXPR17
   std::size_t istrsize(const T* str) {
    static_assert(IsCharType<T>);
    return std::char_traits<T>::length(str);
  }
} // namespace H

template <typename T>
inline EXICPP_CXPR17
 auto strsize(const T& t)
 -> decltype(std::size_t(*(t.size() + t.data()))) {
  return t.size();
}

template <typename T, std::size_t N>
ALWAYS_INLINE constexpr
 std::size_t strsize(const T(&arr)[N]) {
  return N;
}

template <typename T>
inline EXICPP_CXPR17
 std::size_t strsize(const T& maybe_ptr) {
  static_assert(is_stringlike_v<T>,
    "Must pass an object compatible with std::size, or a pointer");
  if EXICPP_UNLIKELY(!maybe_ptr)
    return 0;
  return H::istrsize(maybe_ptr);
}

//////////////////////////////////////////////////////////////////////////

template <typename T>
inline EXICPP_CXPR17
 auto strdata(const T& t)
 -> decltype(t.data()) {
  using DataType = decltype(t.data());
  static_assert(
    std::is_pointer<DataType>::value && is_stringlike_v<DataType>,
    "Must pass an object that returns a string.");
  return t.data();
}

template <typename T>
EXICPP_NODEBUG inline constexpr
 T* strdata(T* ptr) {
  static_assert(IsCharType<T>,
    "Must pass a stringlike object.");
  return ptr;
}

} // namespace exi

#endif // EXIP_TRAITS_HPP
