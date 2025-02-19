//===- Common/D/Char.hpp --------------------------------------------===//
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

#include <string_view>

namespace exi {

/// The default character type.
using char_t = char;
/// Alias for `char_t`.
using Char   = char_t;

/// Default traits for `char_t`.
using CharTraits = std::char_traits<char_t>;
/// Char traits for `wchar_t`.
using WCharTraits = std::char_traits<wchar_t>;

/// Alias for the default `std::string_view` type.
using StrSpan  = std::basic_string_view<char_t, CharTraits>;
/// Alias for `std::wstring_view`.
using WStrSpan = std::wstring_view;

} // namespace exi
