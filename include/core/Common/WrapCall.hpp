//===- Common/WrapCall.hpp ------------------------------------------===//
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

#pragma once

#include <Common/ConstexprLists.hpp>
#include <Common/FunctionTraits.hpp>
#include <Support/IntCast.hpp>
#include <Support/PointerLikeTraits.hpp>

namespace exi {

/// Defines functions for mapping API type -> Wrapped type.
/// For example, in:
///   `void(*Stored)(void*) -> void(*Wrapped)(int*)`
/// You would use `WrapCallTraits<void*, int*>`.
template <typename ExposedT, typename WrappedT>
struct WrapCallTraits;

namespace wrapcall_detail {
template <typename T>
concept is_void_base = std::is_void_v<std::remove_cv_t<T>>;
} // namespace wrapcall_detail

/// Overload for `T` -> `T`.
template <typename T>
struct WrapCallTraits<T, T> {
  static inline T box(T Val) { return EXI_FWD(Val); }
  static inline T unbox(T Val) { return EXI_FWD(Val); }
};

/// Overload for `T` to `void`.
template <typename ExposedT>
requires std::constructible_from<ExposedT>
struct WrapCallTraits<ExposedT, void> {
  static inline ExposedT box() { return ExposedT(); }
  static inline void unbox(ExposedT Val) { }
};

/// Overload for `void*` to a specialized `PointerLikeTypeTraits`.
template <typename WrappedT>
requires is_pointerlike_and_not_uptr<WrappedT>
struct WrapCallTraits<void*, WrappedT> {
  using TraitsT = PointerLikeTypeTraits<WrappedT>;

  static inline void* box(WrappedT Val) {
    auto* const Ptr = TraitsT::getAsVoidPointer(EXI_FWD(Val));
    return const_cast<void*>(Ptr);
  }
  static inline WrappedT unbox(void* Ptr) {
    return TraitsT::getFromVoidPointer(Ptr);
  }
};

/// Overload for `const void*` to a specialized `PointerLikeTypeTraits`.
template <typename WrappedT>
requires is_pointerlike_and_not_uptr<WrappedT>
struct WrapCallTraits<const void*, WrappedT> {
  using NonConst = WrapCallTraits<void*, WrappedT>;

  EXI_INLINE static void* box(WrappedT Val) {
    return NonConst::box(EXI_FWD(Val));
  }
  EXI_INLINE static WrappedT unbox(const void* Ptr) {
    return NonConst::unbox(const_cast<void*>(Ptr));
  }
};

/// Overload for `[const] void*` to a specialized `PointerLikeTypeTraits&`.
template <typename VoidT, typename WrappedT>
requires wrapcall_detail::is_void_base<VoidT>
  && is_pointerlike_and_not_uptr<WrappedT*>
struct WrapCallTraits<VoidT*, WrappedT&> {
  using Pointer = WrapCallTraits<VoidT*, WrappedT*>;

  EXI_INLINE static VoidT* box(WrappedT& Ref EXI_LIFETIMEBOUND) {
    return Pointer::box(&Ref);
  }
  EXI_INLINE static WrappedT& unbox(VoidT* Ptr) {
    exi_invariant(Ptr != nullptr);
    return *Pointer::unbox(Ptr);
  }
};

//////////////////////////////////////////////////////////////////////////
// WrapCallPtr

template <auto Func>
using ptr_function_traits = function_traits<decltype(Func)>;

template <auto Func,
  class Sig = typename ptr_function_traits<Func>::type>
struct WrapCallPtr {
  COMPILE_FAILURE(WrapCallPtr,
    "Invalid wrap, 'Func' may not be a free function.")
};

template <auto Func,
  typename ReturnT, typename...Args>
struct WrapCallPtr<Func, ReturnT(Args...)>
    : public FunctionTraits<ReturnT(*)(Args...)> {
  static constexpr bool is_noexcept
    = has_noexcept_v<decltype(Func)>;
  
  static ReturnT call(Args...args) noexcept(is_noexcept) {
    // TODO: Make this tail_return when musttail supports noexcept.
    return Func(EXI_FWD(args)...);
  }
};

//////////////////////////////////////////////////////////////////////////
// Extras

namespace wrapcall_detail {

/// If return type should be deduced.
struct deduced;

/// Handles `T(void*)`.
template <class WrapperT,
  typename ReturnT = typename WrapperT::result_t,
  bool HasArgs = WrapperT::num_args>
struct Opaque {
  static_assert(WrapperT::num_args == 1,
    "Too many arguments for an opaque call!");
  using ArgT = typename WrapperT::template arg_t<0>;
  using Traits = WrapCallTraits<void*, ArgT>;
  using Wrapper = WrapperT;

  static inline void* wrap(ArgT Val) {
    return Traits::box(EXI_FWD(Val));
  }
  EXI_NO_INLINE static ReturnT call(void* Opaque) {
    ArgT Val = Traits::unbox(Opaque);
    return static_cast<ReturnT>(
      WrapperT::call(EXI_FWD(Val)));
  }
};

/// Overload for when the wrapped type is `U(void)`.
template <class WrapperT, typename ReturnT>
struct Opaque<WrapperT, ReturnT, false>  {
  using Traits = WrapCallTraits<void*, void>;
  using Wrapper = WrapperT;

  ALWAYS_INLINE static void* wrap() {
    return Traits::box();
  }
  EXI_NO_INLINE static ReturnT call(void*) {
    return static_cast<ReturnT>(WrapperT::call());
  }
};

/// Overload for when the return type is `deduced`.
template <class WrapperT, bool HasArgs>
struct Opaque<WrapperT, deduced, HasArgs>
  : Opaque<WrapperT, typename WrapperT::result_t, HasArgs> {};

} // namespace wrapcall_detail

//======================================================================//
// Implementation
//======================================================================//

/// Wraps a function pointer.
template <auto Func, typename Ret = wrapcall_detail::deduced>
using WrapOpaqueCall = wrapcall_detail::Opaque<WrapCallPtr<Func>, Ret>;

} // namespace exi
