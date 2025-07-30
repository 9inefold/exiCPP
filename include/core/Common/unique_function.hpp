//===- Common/unique_function.hpp -----------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2025 Eightfold
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
///
/// \file
/// This file provides unique_function, which works like std::function but
/// supports move-only callable objects and const-qualification.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <Common/PointerIntPair.hpp>
#include <Common/PointerUnion.hpp>
#include <Support/SafeAlloc.hpp>
#include <cstring>
#include <memory>
#include <type_traits>

namespace exi {

/// unique_function is a type-erasing functor similar to std::function.
///
/// It can hold move-only function objects, like lambdas capturing unique_ptrs.
/// Accordingly, it is movable but not copyable.
///
/// It supports const-qualification:
/// - unique_function<int() const> has a const operator().
///   It can only hold functions which themselves have a const operator().
/// - unique_function<int()> has a non-const operator().
///   It can hold functions with a non-const operator(), like mutable lambdas.
template <typename FunctionT> class unique_function;

namespace ufunc_detail {

template <typename T>
concept is_trivial
  =  std::is_trivially_move_constructible_v<T>
  && std::is_trivially_destructible_v<T>;

template <typename CallableT, typename ThisT>
concept not_same = !std::same_as<std::remove_cvref_t<CallableT>, ThisT>;

template <typename CallableRet, typename Ret>
concept is_callable_similar_ret
  =  std::same_as<CallableRet, Ret>
  || std::same_as<const CallableRet, Ret>
  || std::is_convertible_v<CallableRet, Ret>;

template <typename CallableT, typename Ret, typename... Params>
concept is_callable = std::is_void_v<Ret>
  || is_callable_similar_ret<
      decltype(std::declval<CallableT>()(std::declval<Params>()...)), Ret>;

} // namespace ufunc_detail

namespace H {

template <typename ReturnT, typename... ParamTs> class UniqueFunctionBase {
protected:
  static constexpr size_t InlineStorageSize = sizeof(void *) * 3;
  static constexpr size_t InlineStorageAlign = alignof(void *);

  template <typename T, class = void>
  struct IsSizeLessThanThresholdT : std::false_type {};

  template <typename T>
  struct IsSizeLessThanThresholdT<
      T, std::enable_if_t<sizeof(T) <= 2 * sizeof(void *)>> : std::true_type {};

  // Provide a type function to map parameters that won't observe extra copies
  // or moves and which are small enough to likely pass in register to values
  // and all other types to l-value reference types. We use this to compute the
  // types used in our erased call utility to minimize copies and moves unless
  // doing so would force things unnecessarily into memory.
  //
  // The heuristic used is related to common ABI register passing conventions.
  // It doesn't have to be exact though, and in one way it is more strict
  // because we want to still be able to observe either moves *or* copies.
  template <typename T> struct AdjustedParamTBase {
    static_assert(!std::is_reference<T>::value,
                  "references should be handled by template specialization");
    using type =
        std::conditional_t<std::is_trivially_copy_constructible<T>::value &&
                               std::is_trivially_move_constructible<T>::value &&
                               IsSizeLessThanThresholdT<T>::value,
                           T, T &>;
  };

  // This specialization ensures that 'AdjustedParam<V<T>&>' or
  // 'AdjustedParam<V<T>&&>' does not trigger a compile-time error when 'T' is
  // an incomplete type and V a templated type.
  template <typename T> struct AdjustedParamTBase<T &> { using type = T &; };
  template <typename T> struct AdjustedParamTBase<T &&> { using type = T &; };

  template <typename T>
  using AdjustedParamT = typename AdjustedParamTBase<T>::type;

  // The type of the erased function pointer we use as a callback to dispatch to
  // the stored callable when it is trivial to move and destroy.
  using CallPtrT = ReturnT (*)(void *CallableAddr,
                               AdjustedParamT<ParamTs>... Params);
  using MovePtrT = void (*)(void *LHSCallableAddr, void *RHSCallableAddr);
  using DestroyPtrT = void (*)(void *CallableAddr);

  /// A struct to hold a single trivial callback with sufficient alignment for
  /// our bitpacking.
  struct alignas(8) TrivialCallback {
    CallPtrT CallPtr;
  };

  /// A struct we use to aggregate three callbacks when we need full set of
  /// operations.
  struct alignas(8) NonTrivialCallbacks {
    CallPtrT CallPtr;
    MovePtrT MovePtr;
    DestroyPtrT DestroyPtr;
  };

  // Create a pointer union between either a pointer to a static trivial call
  // pointer in a struct or a pointer to a static struct of the call, move, and
  // destroy pointers.
  using CallbackPointerUnionT =
      PointerUnion<const TrivialCallback *, const NonTrivialCallbacks *>;

  // The main storage buffer. This will either have a pointer to out-of-line
  // storage or an inline buffer storing the callable.
  union StorageUnionT {
    // For out-of-line storage we keep a pointer to the underlying storage and
    // the size. This is enough to deallocate the memory.
    struct OutOfLineStorageT {
      void *StoragePtr;
      size_t Size;
      size_t Alignment;
    } OutOfLineStorage;
    static_assert(
        sizeof(OutOfLineStorageT) <= InlineStorageSize,
        "Should always use all of the out-of-line storage for inline storage!");

    // For in-line storage, we just provide an aligned character buffer. We
    // provide three pointers worth of storage here.
    // This is mutable as an inlined `const unique_function<void() const>` may
    // still modify its own mutable members.
    alignas(InlineStorageAlign) mutable std::byte
        InlineStorage[InlineStorageSize];
  } StorageUnion;

  // A compressed pointer to either our dispatching callback or our table of
  // dispatching callbacks and the flag for whether the callable itself is
  // stored inline or not.
  PointerIntPair<CallbackPointerUnionT, 1, bool> CallbackAndInlineFlag;

  bool isInlineStorage() const { return CallbackAndInlineFlag.getInt(); }

  bool isTrivialCallback() const {
    return isa<const TrivialCallback*>(
      CallbackAndInlineFlag.getPointer());
  }

  const CallPtrT getTrivialCallback() const {
    return cast<const TrivialCallback*>(
      CallbackAndInlineFlag.getPointer())->CallPtr;
  }

  const NonTrivialCallbacks *getNonTrivialCallbacks() const {
    return cast<const NonTrivialCallbacks*>(
      CallbackAndInlineFlag.getPointer());
  }

  CallPtrT getCallPtr() const {
    return isTrivialCallback() ? getTrivialCallback()
                               : getNonTrivialCallbacks()->CallPtr;
  }

  // These three functions are only const in the narrow sense. They return
  // mutable pointers to function state.
  // This allows unique_function<T const>::operator() to be const, even if the
  // underlying functor may be internally mutable.
  //
  // const callers must ensure they're only used in const-correct ways.
  void *getCalleePtr() const {
    return isInlineStorage() ? getInlineStorage() : getOutOfLineStorage();
  }
  void *getInlineStorage() const { return &StorageUnion.InlineStorage; }
  void *getOutOfLineStorage() const {
    return StorageUnion.OutOfLineStorage.StoragePtr;
  }

  size_t getOutOfLineStorageSize() const {
    return StorageUnion.OutOfLineStorage.Size;
  }
  size_t getOutOfLineStorageAlignment() const {
    return StorageUnion.OutOfLineStorage.Alignment;
  }

  void setOutOfLineStorage(void *Ptr, size_t Size, size_t Alignment) {
    StorageUnion.OutOfLineStorage = {Ptr, Size, Alignment};
  }

  template <typename CalledAsT>
  static ReturnT CallImpl(void *CallableAddr,
                          AdjustedParamT<ParamTs>... Params) {
    auto &Func = *reinterpret_cast<CalledAsT *>(CallableAddr);
    return Func(std::forward<ParamTs>(Params)...);
  }

  template <typename CallableT>
  static void MoveImpl(void *LHSCallableAddr, void *RHSCallableAddr) noexcept {
    new (LHSCallableAddr)
        CallableT(std::move(*reinterpret_cast<CallableT *>(RHSCallableAddr)));
  }

  template <typename CallableT>
  static void DestroyImpl(void *CallableAddr) noexcept {
    reinterpret_cast<CallableT *>(CallableAddr)->~CallableT();
  }

  // The pointers to call/move/destroy functions are determined for each
  // callable type (and called-as type, which determines the overload chosen).
  // (definitions are out-of-line).

  // By default, we need an object that contains all the different
  // type erased behaviors needed. Create a static instance of the struct type
  // here and each instance will contain a pointer to it.
  // Wrap in a struct to avoid https://gcc.gnu.org/PR71954
  template <typename CallableT, typename CalledAs>
  struct CallbacksHolder {
    static constexpr NonTrivialCallbacks Callbacks = {
      &CallImpl<CalledAs>, &MoveImpl<CallableT>, &DestroyImpl<CallableT>
    };
  };
  // See if we can create a trivial callback. We need the callable to be
  // trivially moved and trivially destroyed so that we don't have to store
  // type erased callbacks for those operations.
  template <typename CallableT, typename CalledAs>
  requires ufunc_detail::is_trivial<CallableT>
  struct CallbacksHolder<CallableT, CalledAs> {
    static constexpr TrivialCallback Callbacks = { &CallImpl<CalledAs> };
  };

  // A simple tag type so the call-as type to be passed to the constructor.
  template <typename T> struct CalledAs {};

  // Essentially the "main" unique_function constructor, but subclasses
  // provide the qualified type to be used for the call.
  // (We always store a T, even if the call will use a pointer to const T).
  template <typename CallableT, typename CalledAsT>
  UniqueFunctionBase(CallableT Callable, CalledAs<CalledAsT>) {
    bool IsInlineStorage = true;
    void *CallableAddr = getInlineStorage();
    if (sizeof(CallableT) > InlineStorageSize ||
        alignof(CallableT) > InlineStorageAlign) {
      IsInlineStorage = false;
      // Allocate out-of-line storage. FIXME: Use an explicit alignment
      // parameter in C++17 mode.
      auto Size = sizeof(CallableT);
      auto Alignment = alignof(CallableT);
      CallableAddr = allocate_buffer(Size, Alignment);
      setOutOfLineStorage(CallableAddr, Size, Alignment);
    }

    // Now move into the storage.
    new (CallableAddr) CallableT(std::move(Callable));
    CallbackAndInlineFlag.setPointerAndInt(
        &CallbacksHolder<CallableT, CalledAsT>::Callbacks, IsInlineStorage);
  }

  ~UniqueFunctionBase() {
    if (!CallbackAndInlineFlag.getPointer())
      return;

    // Cache this value so we don't re-check it after type-erased operations.
    bool IsInlineStorage = isInlineStorage();

    if (!isTrivialCallback())
      getNonTrivialCallbacks()->DestroyPtr(
          IsInlineStorage ? getInlineStorage() : getOutOfLineStorage());

    if (!IsInlineStorage)
      deallocate_buffer(getOutOfLineStorage(), getOutOfLineStorageSize(),
                        getOutOfLineStorageAlignment());
  }

  UniqueFunctionBase(UniqueFunctionBase &&RHS) noexcept {
    // Copy the callback and inline flag.
    CallbackAndInlineFlag = RHS.CallbackAndInlineFlag;

    // If the RHS is empty, just copying the above is sufficient.
    if (!RHS)
      return;

    if (!isInlineStorage()) {
      // The out-of-line case is easiest to move.
      StorageUnion.OutOfLineStorage = RHS.StorageUnion.OutOfLineStorage;
    } else if (isTrivialCallback()) {
      // Move is trivial, just memcpy the bytes across.
      memcpy(getInlineStorage(), RHS.getInlineStorage(), InlineStorageSize);
    } else {
      // Non-trivial move, so dispatch to a type-erased implementation.
      getNonTrivialCallbacks()->MovePtr(getInlineStorage(),
                                        RHS.getInlineStorage());
      getNonTrivialCallbacks()->DestroyPtr(RHS.getInlineStorage());
    }

    // Clear the old callback and inline flag to get back to as-if-null.
    RHS.CallbackAndInlineFlag = {};

#if !defined(NDEBUG) && !EXI_ADDRESS_SANITIZER_BUILD
    // In debug builds without ASan, we also scribble across the rest of the
    // storage. Scribbling under AddressSanitizer (ASan) is disabled to prevent
    // overwriting poisoned objects (e.g., annotated short strings).
    memset(RHS.getInlineStorage(), 0xAD, InlineStorageSize);
#endif
  }

  UniqueFunctionBase &operator=(UniqueFunctionBase &&RHS) noexcept {
    if (this == &RHS)
      return *this;

    // Because we don't try to provide any exception safety guarantees we can
    // implement move assignment very simply by first destroying the current
    // object and then move-constructing over top of it.
    this->~UniqueFunctionBase();
    new (this) UniqueFunctionBase(std::move(RHS));
    return *this;
  }

  UniqueFunctionBase() = default;

public:
  explicit operator bool() const {
    return (bool)CallbackAndInlineFlag.getPointer();
  }
};

} // namespace H

template <typename R, typename... P>
class unique_function<R(P...)> : public H::UniqueFunctionBase<R, P...> {
  using Base = H::UniqueFunctionBase<R, P...>;

public:
  unique_function() = default;
  unique_function(std::nullptr_t) {}
  unique_function(unique_function &&) = default;
  unique_function(const unique_function &) = delete;
  unique_function &operator=(unique_function &&) = default;
  unique_function &operator=(const unique_function &) = delete;

  template <typename CallableT>
  requires ufunc_detail::not_same<CallableT, unique_function>
        && ufunc_detail::is_callable<CallableT, R, P...>
  unique_function(CallableT Callable)
      : Base(std::forward<CallableT>(Callable),
             typename Base::template CalledAs<CallableT>{}) {}

  R operator()(P... Params) {
    return this->getCallPtr()(this->getCalleePtr(), Params...);
  }
};

template <typename R, typename... P>
class unique_function<R(P...) const>
    : public H::UniqueFunctionBase<R, P...> {
  using Base = H::UniqueFunctionBase<R, P...>;

public:
  unique_function() = default;
  unique_function(std::nullptr_t) {}
  unique_function(unique_function &&) = default;
  unique_function(const unique_function &) = delete;
  unique_function &operator=(unique_function &&) = default;
  unique_function &operator=(const unique_function &) = delete;

  template <typename CallableT>
  requires ufunc_detail::not_same<CallableT, unique_function>
        && ufunc_detail::is_callable<const CallableT, R, P...>
  unique_function(CallableT Callable)
      : Base(std::forward<CallableT>(Callable),
             typename Base::template CalledAs<const CallableT>{}) {}

  R operator()(P... Params) const {
    return this->getCallPtr()(this->getCalleePtr(), Params...);
  }
};

} // end namespace exi
