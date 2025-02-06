//===- Common/CRTPTraits.hpp ----------------------------------------===//
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
/// This file defines some helper macros for CRTP.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <concepts>

#ifdef _MSC_VER
# if !defined(_MSVC_TRADITIONAL) || _MSVC_TRADITIONAL
#  error The traditional preprocessor cannot be used, enable /Zc:preprocessor!
# endif // _MSVC_TRADITIONAL
#endif // _MSC_VER

#define EXI_CRTP_EXPAND_(BASE, SUPER) BASE, SUPER
#define EXI_CRTP_EXPAND(PACKED) EXI_CRTP_EXPAND_ PACKED

#define EXI_CHECK_CRTP1(BASE, SUPER, FUNC) \
  (::exi::hasCRTPMethod(&BASE::FUNC, &SUPER::FUNC))
#define EXI_CHECK_CRTP0(...) EXI_CHECK_CRTP1(__VA_ARGS__)

namespace exi {
namespace H {
template <class> struct CRTPTraits;

template <typename Ret, class Obj, typename...Args>
struct TCRTPTraits {
  using return_type = Ret;
  using object_type = Obj;
  static constexpr usize num_args = sizeof...(Args);
};

template <typename Ret, class Obj, typename...Args>
struct CRTPTraits<Ret(Obj::*)(Args...)>
 : TCRTPTraits<Ret, Obj, Args...> {
  template <class NewT> using SwapObj
    = Ret(NewT::*)(Args...);
};

template <typename Ret, class Obj, typename...Args>
struct CRTPTraits<Ret(Obj::*)(Args...) noexcept>
 : TCRTPTraits<Ret, Obj, Args...> {
  template <class NewT> using SwapObj
    = Ret(NewT::*)(Args...) noexcept;
};

template <typename Ret, class Obj, typename...Args>
struct CRTPTraits<Ret(Obj::*)(Args...) const>
 : TCRTPTraits<Ret, Obj, Args...> {
  template <class NewT> using SwapObj
    = Ret(NewT::*)(Args...) const;
};

template <typename Ret, class Obj, typename...Args>
struct CRTPTraits<Ret(Obj::*)(Args...) const noexcept>
 : TCRTPTraits<Ret, Obj, Args...> {
  template <class NewT> using SwapObj
    = Ret(NewT::*)(Args...) const noexcept;
};

template <typename MethodT>
using crtp_object_t = typename CRTPTraits<MethodT>::object_type;

template <typename MethodT, class NewT>
using swap_crtp_object_t
  = typename CRTPTraits<MethodT>::template SwapObj<NewT>;
} // namespace H

/// Checks if methods have equal types excluding the object type.
template <typename BaseM, typename SuperM>
concept kHasDistinctMethodType = std::same_as<
  BaseM, H::swap_crtp_object_t<
    SuperM, H::crtp_object_t<BaseM>
  >
>;

/// Checks if a CRTP method has been implemented by the derived type.
template <typename BaseM, typename SuperM>
constexpr bool hasCRTPMethod(BaseM BM, SuperM SM) noexcept {
  if constexpr (kHasDistinctMethodType<BaseM, SuperM>)
    // Same signature, check if the pointers differ.
    return (BM != SM);
  // Differing signatures, must have been implemented.
  return true;
}

} // namespace exi

//////////////////////////////////////////////////////////////////////////

/// This macro should be used to verify a CRTP derived class has implemented a
/// given function. You must define the `EXI_CRTP_TYPES` before any uses, which
/// must be in the form `(BASE, SUPER)`. If a type has commas in it, you must
/// define a typedef/using. Example for a class `X<T>`:
///
/// #define EXI_CRTP_TYPES (X<T>, T)
/// void foo(int I) {
///   if constexpr EXI_CRTP_CHECK(foo)
///     tail_return super()->foo(I);
/// }
/// #undef EXI_CRTP_TYPES
///
#define EXI_CRTP_CHECK(FUNC) EXI_CHECK_CRTP0(   \
  EXI_CRTP_EXPAND(EXI_CRTP_TYPES), FUNC)

/// This macro should be used to assert a CRTP derived class has implemented
/// a given function. Example:
///
/// EXI_CRTP_ASSERT(foo, "Must implement foo!");
/// EXI_CRTP_ASSERT(bar);
///
#define EXI_CRTP_ASSERT(FUNC, ...)              \
  static_assert(EXI_CRTP_CHECK(FUNC)            \
    __VA_OPT__(,) __VA_ARGS__)

/// This macro should be used to implement `super()` for accesses of supertypes.
///
#define EXI_CRTP_DEFINE_SUPER(SUPER)            \
SUPER* super() {                                \
  return static_cast<SUPER*>(this);             \
}                                               \
const SUPER* super() const {                    \
  return static_cast<const SUPER*>(this);       \
}
