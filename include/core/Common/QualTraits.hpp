//===- Common/QualTraits.hpp ----------------------------------------===//
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
/// This file defines some helper traits for qualifiers.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <concepts>

namespace exi {

//////////////////////////////////////////////////////////////////////////
// CopyCV

namespace H {
template <class To, class From>
struct CopyCV {
  using type = To;
};

template <class To, class From>
struct CopyCV<To, const From> {
  using type = const To;
};

template <class To, class From>
struct CopyCV<To, volatile From> {
  using type = volatile To;
};

template <class To, class From>
struct CopyCV<To, const volatile From> {
  using type = const volatile To;
};
} // namespace H

template <class To, class From = void>
using copy_cv_t = typename H::CopyCV<To, From>::type;

//////////////////////////////////////////////////////////////////////////
// CopyRef

namespace H {
template <class To, class From>
struct CopyRef {
  using type = To;
};

template <class To, class From>
struct CopyRef<To, From&> {
  using type = To&;
};

template <class To, class From>
struct CopyRef<To, From&&> {
  using type = To&&;
};
} // namespace H

template <class To, class From = void>
using copy_ref_t = typename H::CopyRef<To, From>::type;

//////////////////////////////////////////////////////////////////////////
// CopyQuals

namespace H {
template <class To, class From>
struct CopyQuals {
  using Base = std::remove_cvref_t<To>;
  using CV = copy_cv_t<Base, std::remove_reference_t<From>>;
  using type = copy_ref_t<CV, From>;
};
} // namespace H

template <class To, class From = void>
using copy_quals_t = typename H::CopyQuals<To, From>::type;

template <class To, class From = void>
using copy_cvref_t = copy_quals_t<To, From>;

//===----------------------------------------------------------------===//
// Signedness
//===----------------------------------------------------------------===//

namespace H {
template <typename T> struct SwapSign {
	using type = T;
};

template <typename T>
requires std::signed_integral<T>
struct SwapSign<T> {
	using type = std::make_unsigned_t<T>;
};

template <typename T>
requires std::unsigned_integral<T>
struct SwapSign<T> {
	using type = std::make_signed_t<T>;
};
} // namespace H

template <typename T>
using swap_sign_t = typename H::SwapSign<T>::type;

} // namespace exi
