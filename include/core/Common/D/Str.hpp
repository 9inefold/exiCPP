//===- Common/D/Str.hpp ---------------------------------------------===//
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

#include <Common/D/Char.hpp>
#include <Support/Alloc.hpp>
#include <string>

namespace exi {

/// Alias for the `std::string` type with `char_t` and our allocator.
using String  = std::basic_string<char_t, CharTraits, Allocator<char_t>>;
/// Alias for `std::wstring` with our allocator.
using WString = std::basic_string<wchar_t, WCharTraits, Allocator<wchar_t>>;

} // namespace exi
