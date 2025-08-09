//===- Common/D/TypePackElement.hpp ---------------------------------===//
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
/// This file defines TypePackElement.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <type_traits>
#include <utility>

namespace exi {

#if EXI_HAS_BUILTIN(__type_pack_element)
# define EXI_TYPEPACKELEMENT __type_pack_element
template <usize I, typename...TT>
using TypePackElement = __type_pack_element<I, TT...>;
#else
# define EXI_TYPEPACKELEMENT ::exi::TypePackElement
template <usize I, typename...TT>
using TypePackElement = std::tuple_element_t<I, std::tuple<TT...>>;
#endif

template <typename...TT>
using TypePackLast = EXI_TYPEPACKELEMENT<sizeof...(TT) - 1, TT...>;

} // namespace exi
