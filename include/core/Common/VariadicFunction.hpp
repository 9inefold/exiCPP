//===- Common/VariadicFunction.hpp ----------------------------------===//
//
// Copyright (C) 2025 Eightfold
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
/// This file implements variadic_function, a utility for generating overloads.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/ArrayRef.hpp>
#include <Common/ConstexprLists.hpp>

// TODO: Add versions with different signatures?

namespace exi {
namespace vfunc_detail {

template <typename ArgT>
using vargs_t = ArrayRef<const ArgT*>;

////////////////////////////////////////////////////////////////////////////////
// Traits

template <typename T>
struct IsVArgsArrayRef : std::false_type {};
template <typename T>
struct IsVArgsArrayRef<ArrayRef<const T*>> : std::true_type { using type = T; };
template <typename ArgT>
concept is_vargs_arrayref = IsVArgsArrayRef<ArgT>::value;

template <typename...ArgsT>
concept has_trailing_arrayref = is_vargs_arrayref<TypePackLast<ArgsT...>>;

template <typename...ArgsT>
concept is_valid_signature
  = (sizeof...(ArgsT) != 0)
  && has_trailing_arrayref<ArgsT...>;

////////////////////////////////////////////////////////////////////////////////
// Splitting

template <class Seq, typename...ArgsT>
struct SplitPack;

template <size_t...II, typename...ArgsT>
struct SplitPack<idxseq<II...>, ArgsT...> {
  using type = typeseq<
    TypePackLast<ArgsT...>,
    EXI_TYPEPACKELEMENT<II, ArgsT...>...>;
};

template <typename...ArgsT>
using split_pack_t = typename SplitPack<
  make_idxseq<sizeof...(ArgsT) - 1>, ArgsT...>::type;

////////////////////////////////////////////////////////////////////////////////
// Bases

template <class SeqT, class ListT, auto* Fn>
struct VArgsBase;

template <typename ArgT, typename...HeadT, auto* Fn>
struct VArgsBase<idxseq<>, typeseq<ArgT, HeadT...>, Fn> {
  EXI_NODEBUG constexpr auto operator()(HeadT...Head) const {
    return Fn(Head..., ArgT{});
  }
};

template <size_t I, size_t...II,
  typename ArgT, typename...HeadT, auto* Fn>
struct VArgsBase<idxseq<I, II...>, typeseq<ArgT, HeadT...>, Fn> {
  using ArgTy = typename IsVArgsArrayRef<ArgT>::type;
  EXI_NODEBUG constexpr auto operator()(HeadT...Head,
      const ArgTy& Arg0, Unfold<const ArgTy&, II>...Args) const {
    const ArgTy* const ArgArr[] {&Arg0, &Args...};
    return Fn(Head..., ArgT(ArgArr));
  }
};

template <size_t I, class ListT, auto* Fn>
using vargs_base_t = VArgsBase<make_idxseq<I>, ListT, Fn>;

template <class SeqT, class ListT, auto* Fn>
struct VArgsImpl;

template <size_t...II, class ListT, auto* Fn>
struct VArgsImpl<idxseq<II...>, ListT, Fn>
    : public vargs_base_t<II, ListT, Fn>... {
  using vargs_base_t<II, ListT, Fn>::operator()...;
};

template <class ListT, auto* Fn, size_t ExpandLimit>
using vargs_impl_t = VArgsImpl<make_idxseq<ExpandLimit>, ListT, Fn>;

} // namespace vfunc_detail

////////////////////////////////////////////////////////////////////////////////
// Implementation

inline constexpr size_t kVArgsExpandLimit = 32;

/// The `variadic_function` class template makes it easy to define
/// type-safe variadic functions where all arguments have the same
/// type.
///
/// Suppose we need a variadic function like this:
///
///   ResultT Foo(const ArgT& A_0, const ArgT& A_1, ..., const ArgT& A_N);
///
/// Instead of many overloads of Foo(), we only need to define a helper
/// function that takes an array of arguments:
///
///   ResultT FooImpl(ArrayRef<const ArgT*> Args) {
///     // 'Args[i]' is a pointer to the i-th argument passed to Foo().
///     ...
///   }
///
/// and then define Foo() like this:
///
///   constexpr variadic_function<&FooImpl> Foo;
///
/// `variadic_function` takes care of defining the overloads of Foo().
///
/// Actually, Foo is a function object (i.e. functor) instead of a plain
/// function.  This object is stateless and its constructor/destructor
/// does nothing, so it's safe to create global objects and call Foo(...) at
/// any time.
///
/// It can also handle cases with fixed leading arguments. For example:
///
///   bool FullMatchImpl(const StringRef& S, const RE& Regex,
///                      ArrayRef<const ArgT*> Args);
///
///   constexpr variadic_function<&FullMatchImpl> FullMatch;
///
/// By default, `variadic_function` will generate overloads for up to
/// `kVArgsExpandLimit` arguments, but you can specify an amount with the second
/// argument:
///
///   constexpr variadic_function<&FullMatchImpl, 8> FullMatch;
///
template <auto* Fn, size_t ExpandLimit = kVArgsExpandLimit>
struct variadic_function;

template <typename Ret, typename...ArgsT,
  Ret(*Fn)(ArgsT...), size_t ExpandLimit>
requires vfunc_detail::is_valid_signature<ArgsT...>
struct variadic_function<Fn, ExpandLimit>
    : public vfunc_detail::vargs_impl_t<
        vfunc_detail::split_pack_t<ArgsT...>, Fn, ExpandLimit + 1> {
  static_assert(ExpandLimit > 0);
  using vfunc_detail::vargs_impl_t<
    vfunc_detail::split_pack_t<ArgsT...>,
      Fn, ExpandLimit + 1>::operator();
};

template <auto* Fn, size_t ExpandLimit = kVArgsExpandLimit>
inline constexpr variadic_function<Fn, ExpandLimit> VariadicFunction;

} // namespace exi
