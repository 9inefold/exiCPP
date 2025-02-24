//===- Common/PointerUnion.hpp ---------------------------------------===//
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
///
/// \file
/// This file defines the PointerUnion class.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/ConstexprLists.hpp>
#include <Common/DenseMapInfo.hpp>
#include <Common/PointerIntPair.hpp>
#include <Common/STLExtras.hpp>
#include <Support/Casting.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/PointerLikeTraits.hpp>
#include <algorithm>
#include <cassert>

namespace exi {
namespace ptrunion_detail {
  /// Determine the number of bits required to store integers with values < n.
  /// This is ceil(log2(n)).
  constexpr int bitsRequired(unsigned n) {
    return n > 1 ? 1 + bitsRequired((n + 1) / 2) : 0;
  }

  template <typename... Ts> constexpr int lowBitsAvailable() {
    return std::min<int>({PointerLikeTypeTraits<Ts>::NumLowBitsAvailable...});
  }

  /// Find the first type in a list of types.
  template <typename T, typename...> struct GetFirstType {
    using type = T;
  };

  /// Provide PointerLikeTypeTraits for void* that is used by PointerUnion
  /// for the template arguments.
  template <typename ...PTs> class PointerUnionUIntTraits {
  public:
    static inline void *getAsVoidPointer(void *P) { return P; }
    static inline void *getFromVoidPointer(void *P) { return P; }
    static constexpr int NumLowBitsAvailable = lowBitsAvailable<PTs...>();
  };

  template <typename Derived, typename ValT, int I, typename...Types>
  class PointerUnionMembers;

  template <typename Derived, typename ValT, int I>
  class PointerUnionMembers<Derived, ValT, I> {
  protected:
    ValT Val;
    PointerUnionMembers() = default;
    PointerUnionMembers(ValT Val) : Val(Val) {}

    friend struct PointerLikeTypeTraits<Derived>;
  };

  template <typename Derived, typename ValT, int I, typename Type,
            typename...Types>
  class PointerUnionMembers<Derived, ValT, I, Type, Types...>
      : public PointerUnionMembers<Derived, ValT, I + 1, Types...> {
    using Base = PointerUnionMembers<Derived, ValT, I + 1, Types...>;
  public:
    using Base::Base;
    PointerUnionMembers() = default;
    PointerUnionMembers(Type V EXI_LIFETIMEBOUND)
        : Base(ValT(const_cast<void *>(
                         PointerLikeTypeTraits<Type>::getAsVoidPointer(V)),
                     I)) {}

    using Base::operator=;
    Derived &operator=(Type V EXI_LIFETIMEBOUND) {
      this->Val = ValT(
          const_cast<void *>(PointerLikeTypeTraits<Type>::getAsVoidPointer(V)),
          I);
      return static_cast<Derived &>(*this);
    };
  };
} // namespace ptrunion_detail

// This is a forward declaration of CastInfoPointerUnionImpl.
// Refer to its definition below for further details.
template <typename... PTs> struct CastInfoPointerUnionImpl;

/// A discriminated union of two or more pointer types, with the discriminator
/// in the low bit of the pointer.
///
/// This implementation is extremely efficient in space due to leveraging the
/// low bits of the pointer, while exposing a natural and type-safe API.
///
/// Common use patterns would be something like this:
///    PointerUnion<int*, float*> P;
///    P = (int*)0;
///    printf("%d %d", isa<int*>(P), isa<float*>(P));  // prints "1 0"
///    X = cast<int*>(P);     // ok.
///    Y = cast<float*>(P);   // runtime assertion failure.
///    Z = cast<double*>(P);  // compile time failure.
///    P = (float*)0;
///    Y = cast<float*>(P);   // ok.
///    X = cast<int*>(P);     // runtime assertion failure.
///    PointerUnion<int*, int*> Q; // compile time failure.
template <typename... PTs>
class PointerUnion
    : public ptrunion_detail::PointerUnionMembers<
          PointerUnion<PTs...>,
          PointerIntPair<
              void *, ptrunion_detail::bitsRequired(sizeof...(PTs)), int,
              ptrunion_detail::PointerUnionUIntTraits<PTs...>>,
          0, PTs...> {
  static_assert(TypesAreDistinct<PTs...>::value,
                "PointerUnion alternative types cannot be repeated");
  // The first type is special because we want to directly cast a pointer to a
  // default-initialized union to a pointer to the first type. But we don't
  // want PointerUnion to be a 'template <typename First, typename ...Rest>'
  // because it's much more convenient to have a name for the whole pack. So
  // split off the first type here.
  using First = TypeAtIndex<0, PTs...>;
  using Base = typename PointerUnion::PointerUnionMembers;

  /// This is needed to give the CastInfo implementation below access
  /// to protected members.
  /// Refer to its definition for further details.
  friend struct CastInfoPointerUnionImpl<PTs...>;

public:
  PointerUnion() = default;

  PointerUnion(std::nullptr_t) : PointerUnion() {}
  using Base::Base;

  /// Test if the pointer held in the union is null, regardless of
  /// which type it is.
  bool isNull() const { return !this->Val.getPointer(); }

  explicit operator bool() const { return !isNull(); }

  /// If the union is set to the first pointer type get an address pointing to
  /// it.
  First const *getAddrOfPtr1() const {
    return const_cast<PointerUnion *>(this)->getAddrOfPtr1();
  }

  /// If the union is set to the first pointer type get an address pointing to
  /// it.
  First *getAddrOfPtr1() {
    exi_assert(isa<First>(*this) && "Val is not the first pointer");
    exi_assert(
        PointerLikeTypeTraits<First>::getAsVoidPointer(cast<First>(*this)) ==
            this->Val.getPointer() &&
        "Can't get the address because PointerLikeTypeTraits changes the ptr");
    return const_cast<First *>(
        reinterpret_cast<const First *>(this->Val.getAddrOfPointer()));
  }

  /// Assignment from nullptr which just clears the union.
  const PointerUnion &operator=(std::nullptr_t) {
    this->Val.initWithPointer(nullptr);
    return *this;
  }

  /// Assignment from elements of the union.
  using Base::operator=;

  void *getOpaqueValue() const { return this->Val.getOpaqueValue(); }
  static inline PointerUnion getFromOpaqueValue(void *VP) {
    PointerUnion V;
    V.Val = decltype(V.Val)::getFromOpaqueValue(VP);
    return V;
  }
};

template <typename ...PTs>
bool operator==(PointerUnion<PTs...> lhs, PointerUnion<PTs...> rhs) {
  return lhs.getOpaqueValue() == rhs.getOpaqueValue();
}

template <typename ...PTs>
bool operator!=(PointerUnion<PTs...> lhs, PointerUnion<PTs...> rhs) {
  return lhs.getOpaqueValue() != rhs.getOpaqueValue();
}

template <typename ...PTs>
bool operator<(PointerUnion<PTs...> lhs, PointerUnion<PTs...> rhs) {
  return lhs.getOpaqueValue() < rhs.getOpaqueValue();
}

/// We can't (at least, at this moment with C++14) declare CastInfo
/// as a friend of PointerUnion like this:
/// ```
///   template<typename To>
///   friend struct CastInfo<To, PointerUnion<PTs...>>;
/// ```
/// The compiler complains 'Partial specialization cannot be declared as a
/// friend'.
/// So we define this struct to be a bridge between CastInfo and
/// PointerUnion.
template <typename... PTs> struct CastInfoPointerUnionImpl {
  using From = PointerUnion<PTs...>;

  template <typename To> static inline bool isPossible(From &F) {
    return F.Val.getInt() == FirstIndexOfType<To, PTs...>::value;
  }

  template <typename To> static To doCast(From &F) {
    assert(isPossible<To>(F) && "cast to an incompatible type!");
    return PointerLikeTypeTraits<To>::getFromVoidPointer(F.Val.getPointer());
  }
};

// Specialization of CastInfo for PointerUnion
template <typename To, typename... PTs>
struct CastInfo<To, PointerUnion<PTs...>>
    : public DefaultDoCastIfPossible<To, PointerUnion<PTs...>,
                                     CastInfo<To, PointerUnion<PTs...>>> {
  using From = PointerUnion<PTs...>;
  using Impl = CastInfoPointerUnionImpl<PTs...>;

  static inline bool isPossible(From &f) {
    return Impl::template isPossible<To>(f);
  }

  static To doCast(From &f) { return Impl::template doCast<To>(f); }

  static inline To castFailed() { return To(); }
};

// Teach SmallPtrSet that PointerUnion is "basically a pointer", that has
// # low bits available = min(PT1bits,PT2bits)-1.
template <typename ...PTs>
struct PointerLikeTypeTraits<PointerUnion<PTs...>> {
  static inline void *getAsVoidPointer(const PointerUnion<PTs...> &P) {
    return P.getOpaqueValue();
  }

  static inline PointerUnion<PTs...> getFromVoidPointer(void *P) {
    return PointerUnion<PTs...>::getFromOpaqueValue(P);
  }

  // The number of bits available are the min of the pointer types minus the
  // bits needed for the discriminator.
  static constexpr int NumLowBitsAvailable = PointerLikeTypeTraits<decltype(
      PointerUnion<PTs...>::Val)>::NumLowBitsAvailable;
};

// Teach DenseMap how to use PointerUnions as keys.
template <typename ...PTs> struct DenseMapInfo<PointerUnion<PTs...>> {
  using Union = PointerUnion<PTs...>;
  using FirstInfo =
      DenseMapInfo<typename ptrunion_detail::GetFirstType<PTs...>::type>;

  static inline Union getEmptyKey() { return Union(FirstInfo::getEmptyKey()); }

  static inline Union getTombstoneKey() {
    return Union(FirstInfo::getTombstoneKey());
  }

  static unsigned getHashValue(const Union &UnionVal) {
    intptr_t key = (intptr_t)UnionVal.getOpaqueValue();
    return DenseMapInfo<intptr_t>::getHashValue(key);
  }

  static bool isEqual(const Union &LHS, const Union &RHS) {
    return LHS == RHS;
  }
};

} // namespace exi
