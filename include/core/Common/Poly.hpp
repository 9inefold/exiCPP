//===- Common/Option.hpp --------------------------------------------===//
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
/// This file defines the Poly class, which is an inline polymorphic container.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/ConstexprLists.hpp>
#include <Common/Fundamental.hpp>
#include <Common/Option.hpp>
#include <Common/STLExtras.hpp>
#include <Support/AlignedUnion.hpp>
#include <Support/Casting.hpp>
#include <Support/ErrorHandle.hpp>
#include <new>
#include <type_traits>

// TODO: In the future, add a check for embedded llvm-style RTTI.

namespace exi {

template <class Base,
  std::derived_from<Base>...Derived>
class Poly;

namespace poly_detail {

using option_detail::abstract;
using option_detail::concrete;

template <typename T>
concept polymorphic = std::is_polymorphic_v<T>;

template <typename T>
concept copyable = abstract<T> || std::copy_constructible<T>;

template <typename T>
concept movable = abstract<T> || std::move_constructible<T>;

template <typename...TT>
concept all_copyable = (... && copyable<TT>);

template <typename...TT>
concept all_movable = (... && movable<TT>);

////////////////////////////////////////////////////////////////////////////////

template <typename...TT>
concept has_common_reference = requires {
  typename std::common_reference<TT...>::type;
};

template <typename...TT> struct CommonReference {
  COMPILE_FAILURE(CommonReference, "The inputs have no common type!");
  using type = void;
};

template <typename...TT>
requires has_common_reference<TT...>
struct CommonReference<TT...> {
  using type = std::common_reference_t<TT...>;
};

template <typename...TT>
using common_ref = typename CommonReference<TT...>::type;

////////////////////////////////////////////////////////////////////////////////

template <typename Ret, class Obj, class...TT>
struct VisitReturn { using type = Ret; };

template <class Obj, class...TT>
struct VisitReturn<dummy_t, Obj, TT...> {
  using type = common_ref<std::invoke_result_t<Obj&, TT&>...>;
};

template <typename Ret, class Obj, class...TT>
using visit_ret = typename VisitReturn<Ret, Obj, TT...>::type;

////////////////////////////////////////////////////////////////////////////////

/// Casts and launders a pointer to the requested type.
template <typename Out, typename In>
[[nodiscard]] constexpr Out* launder_cast(In* Ptr) noexcept {
  return std::launder(reinterpret_cast<Out*>(Ptr));
}

template <class T, int I>
struct PolyLeaf {
  static consteval type_c<T> at(idx_c<I>) { return {}; }
  static consteval idx_c<I> at(type_c<T>) { return {}; }
};

template <class Seq, class...TT>
struct PolyBranch;

template <usize...II, class...TT>
struct EXI_EMPTY_BASES
  PolyBranch<idxseq<II...>, TT...>
    : public PolyLeaf<TT, II>... {
  using PolyLeaf<TT, II>::at...;
};

template <class Base, class...Derived>
using poly_branch = PolyBranch<
  make_idxseq<sizeof...(Derived) + 1>,
  Base, Derived...
>;

template <class Base, class...Derived>
class EXI_EMPTY_BASES Storage
    : public poly_branch<Base, Derived...> {
protected:
  using poly_branch<Base, Derived...>::at;
  AlignedCharArrayUnion<Base, Derived...> Data;
  int Tag = 0;
public:
  constexpr Storage() : Data() {}

  template <typename U>
  requires is_one_of<U, Base, Derived...> && concrete<U>
  Storage(std::in_place_type_t<U>, auto&&...Args)
      : Tag(tagof(type_c<U>{})) {
    new (Data.buffer) U(EXI_FWD(Args)...);
  }

  static constexpr decltype(auto)
   From(Poly<Base, Derived...>& Child) noexcept {
    return static_cast<Storage&>(Child);
  }

  static constexpr decltype(auto)
   From(const Poly<Base, Derived...>& Child) noexcept {
    return static_cast<const Storage&>(Child);
  }

  template <typename U>
  requires is_one_of<U, Base, Derived...>
  static constexpr int tagof(type_c<U>) noexcept {
    return at(type_c<U>{}) + 1;
  }

  /// Checks if the object holds the requested type.
  template <typename U>
  requires is_one_of<U, Base, Derived...>
  ALWAYS_INLINE constexpr bool is() const noexcept {
    constexpr_static int UTag = tagof(type_c<U>{});
    if constexpr (!abstract<U>)
      return this->Tag == UTag;
    else
      // Abstract types can never be constructed.
      return false;
  }

  constexpr bool empty() const noexcept {
    return this->Tag == 0;
  }

  /// Gets a pointer to the base object.
  Base* base() noexcept {
    return launder_cast<Base>(Data.buffer);
  }

  /// Gets a const pointer to the base object.
  const Base* base() const noexcept {
    return launder_cast<const Base>(Data.buffer);
  }

  /// Gets a pointer to the object of type `U`.
  template <typename U>
  requires is_one_of<U, Base, Derived...>
  U& as() noexcept {
    exi_invariant(is<U>());
    return *launder_cast<U>(Data.buffer);
  }

  /// Gets a const pointer to the object of type `U`.
  template <typename U>
  requires is_one_of<U, Base, Derived...>
  const U& as() const noexcept {
    exi_invariant(is<U>());
    return *launder_cast<const U>(Data.buffer);
  }

  template <typename U>
  requires is_one_of<U, Base, Derived...>
  void emplace(auto&&...Args) {
    this->Tag = tagof(type_c<U>{});
    new (Data.buffer) U(EXI_FWD(Args)...);
  }

  template <typename U>
  requires is_one_of<U, Base, Derived...>
  void emplace(std::in_place_type_t<U>, auto&&...Args) {
    this->Tag = tagof(type_c<U>{});
    new (Data.buffer) U(EXI_FWD(Args)...);
  }
};

struct Dtor {
  template <typename T>
  constexpr void operator()(T& Val) const noexcept {
    Val.~T();
  }
};

} // namespace poly_detail

template <class Base,
  std::derived_from<Base>...Derived>
class Poly final : private poly_detail::Storage<Base, Derived...> {
  static_assert((... && poly_detail::concrete<Derived>),
    "All derived types must be concrete!");

  friend class poly_detail::Storage<Base, Derived...>;
  using Super = poly_detail::Storage<Base, Derived...>;

  /// Finds the real return type of a visit.
  template <typename R, typename F>
  using visit_t = poly_detail::visit_ret<R, F, Base, Derived...>;

  /// Finds the real return type of a visit.
  template <typename R, typename F>
  using cvisit_t = poly_detail::visit_ret<R, F,
    const Base, std::add_const_t<Derived>...>;

public:
  constexpr Poly() : Super() {}
  constexpr Poly(std::nullopt_t) : Super() {}

  Poly(const Poly& O)
   requires poly_detail::all_copyable<Base, Derived...> : Super() {
    this->operator=(O);
  }

  Poly(Poly&& O) requires poly_detail::all_movable<Base, Derived...> : Super() {
    this->operator=(std::move(O));
  }

  template <typename U>
  Poly(std::in_place_type_t<U>, auto&&...Args) : 
   Super(std::in_place_type<U>, EXI_FWD(Args)...) {
  }

  template <is_one_of<Base, Derived...> U>
  Poly(const U& Value) :
   Super(std::in_place_type<U>, Value) {
  }

  template <is_one_of<Base, Derived...> U>
  Poly(U&& Value) :
   Super(std::in_place_type<U>, std::move(Value)) {
  }

  Poly& operator=(const Poly& O)
   requires poly_detail::all_copyable<Base, Derived...> {
    this->reset();
    if EXI_LIKELY(!O.empty()) {
      O.visitSelf<void>([this] <typename T> (const T& Val) {
        this->Super::template emplace<T>(Val);
      });
    }
    return *this;
  }

  Poly& operator=(Poly&& O) noexcept
   requires poly_detail::all_movable<Base, Derived...> {
    this->reset();
    if EXI_LIKELY(!O.empty()) {
      O.visitSelf<void>([this] <typename T> (T& Val) {
        this->Super::template emplace<T>(std::move(Val));
      });
    }
    O.reset();
    return *this;
  }

  template <is_one_of<Base, Derived...> U>
  Poly& operator=(const U& Value) {
    this->destroy();
    Super::template emplace<U>(Value);
    return *this;
  }

  template <is_one_of<Base, Derived...> U>
  Poly& operator=(U&& Value) {
    this->destroy();
    Super::template emplace<U>(std::move(Value));
    return *this;
  }

  ~Poly() { this->destroy(); }

  using Super::empty;
  constexpr bool has_value() const noexcept { return empty(); }

  /// Visits the active type with `Func`.
  template <typename R = dummy_t, class F>
  auto visit(F&& Func) -> visit_t<R, F>;

  /// Visits the active type with `Func`.
  template <typename R = dummy_t, class F>
  auto visit(F&& Func) const -> cvisit_t<R, F>;

  /// Constructs a new object of type `U`.
  template <typename U>
  requires is_one_of<U, Base, Derived...>
  void emplace(auto&&...Args) {
    this->destroy();
    Super::template emplace<U>(EXI_FWD(Args)...);
  }

  /// Constructs a new object of type `U`.
  template <typename U>
  requires is_one_of<U, Base, Derived...>
  void emplace(std::in_place_type_t<U>, auto&&...Args) {
    this->destroy();
    Super::template emplace<U>(EXI_FWD(Args)...);
  }

  void reset() {
    this->destroy();
    Super::Tag = 0;
  }

  Base* operator->() & noexcept
   requires poly_detail::polymorphic<Base> {
    return Super::base();
  }

  const Base* operator->() const& noexcept
   requires poly_detail::polymorphic<Base> {
    return Super::base();
  }

private:
  void destroy() noexcept {
    if (!empty())
      visitSelf<void>(poly_detail::Dtor{});
  }

  template <class U, typename Ret>
  EXI_NODEBUG ALWAYS_INLINE bool Case(auto& Func) {
    if (Super::template is<U>()) {
      if constexpr (poly_detail::concrete<U>)
        Func(Super::template as<U>());
      return true;
    }
    return false;
  }

  template <class U, typename Ret>
  EXI_NODEBUG ALWAYS_INLINE bool Case(auto& Func) const {
    if (Super::template is<U>()) {
      if constexpr (poly_detail::concrete<U>)
        Func(Super::template as<U>());
      return true;
    }
    return false;
  }

  template <class U, typename Ret>
  EXI_NODEBUG ALWAYS_INLINE bool Case(
   auto& Func, Option<Ret>& Out) {
    if (Super::template is<U>()) {
      if constexpr (poly_detail::concrete<U>)
        Out.emplace(Func(Super::template as<U>()));
      return true;
    }
    return false;
  }

  template <class U, typename Ret>
  EXI_NODEBUG ALWAYS_INLINE bool Case(
   auto& Func, Option<Ret>& Out) const {
    if (Super::template is<U>()) {
      if constexpr (poly_detail::concrete<U>)
        Out.emplace(Func(Super::template as<U>()));
      return true;
    }
    return false;
  }

  template <typename Ret>
  ALWAYS_INLINE bool Default() const { return true; }

  template <typename Ret>
  ALWAYS_INLINE bool Default(Option<Ret>& Out) const {
    if constexpr (std::default_initializable<Ret>) {
      Out.emplace();
      return true;
    }

    exi_unreachable("Hit unreachable in Poly::visit!");
  }

  template <typename Ret>
  EXI_INLINE Ret visitSelf(auto&& Func) {
    if constexpr (std::is_void_v<Ret>) {
      Case<Base, Ret>(Func)
        || (... || Case<Derived, Ret>(Func))
        || Default<Ret>();
    } else {
      Option<Ret> Out;
      Case<Base>(Func, Out)
        || (... || Case<Derived>(Func, Out))
        || Default(Out);
      return std::move(Out).value();
    }
  }

  template <typename Ret>
  EXI_INLINE Ret visitSelf(auto&& Func) const {
    if constexpr (std::is_void_v<Ret>) {
      Case<Base, Ret>(Func)
        || (... || Case<Derived, Ret>(Func))
        || Default<Ret>();
    } else {
      Option<Ret> Out;
      Case<Base>(Func, Out)
        || (... || Case<Derived>(Func, Out))
        || Default(Out);
      return std::move(Out).value();
    }
  }
};

template <class Base,
  std::derived_from<Base>...Derived>
template <typename R, class F>
auto Poly<Base, Derived...>::visit(F&& Func)
 -> Poly<Base, Derived...>::template visit_t<R, F> {
  using Ret = visit_t<R, F>;
  return visitSelf<Ret>(Func);
}

template <class Base,
  std::derived_from<Base>...Derived>
template <typename R, class F>
auto Poly<Base, Derived...>::visit(F&& Func) const
 -> Poly<Base, Derived...>::template cvisit_t<R, F> {
  using Ret = cvisit_t<R, F>;
  return visitSelf<Ret>(Func);
}

//===----------------------------------------------------------------===//
// Cast Traits
//===----------------------------------------------------------------===//

// Specialization of CastInfo for Poly
template <typename To, typename...TT>
requires is_one_of<To, TT...>
struct CastInfo<To, Poly<TT...>>
    : public DefaultDoCastIfPossible<To, Poly<TT...>&,
                            CastInfo<To, Poly<TT...>>> {
  using From = Poly<TT...>;
  using Super = poly_detail::Storage<TT...>;

  static inline bool isPossible(From& Val) {
    auto& Base = Super::From(Val);
    return Base.template is<To>();
  }

  static To& doCast(From& Val) {
    auto& Base = Super::From(Val);
    auto& Data = Base.template as<To>();
    return static_cast<To&>(Data);
  }

  static inline auto castFailed() {
    return std::nullopt;
  }
};

template <typename To, typename...TT>
requires is_one_of<To, TT...>
struct isa_impl<To, Poly<TT...>> {
  using From = Poly<TT...>;
  using Super = poly_detail::Storage<TT...>;

  static inline bool doit(const From &Val) {
    auto& Base = Super::From(Val);
    return Base.template is<To>(); 
  }
};

//template <typename To, typename...TT>
//struct CastInfo<To, const Poly<TT...>,
//	std::enable_if_t<!is_simple_type<const Poly<TT...>>::value>>
//    : public ConstStrippingForwardingCast<To, const Poly<TT...>,
//                            		 CastInfo<To, Poly<TT...>>> {};

} // namespace exi
