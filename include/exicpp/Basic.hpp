//===- exicpp/Basic.hpp ---------------------------------------------===//
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
//
//  Defines types used by most parts of the program.
//
//===----------------------------------------------------------------===//

#pragma once

#ifndef EXIP_BASIC_HPP
#define EXIP_BASIC_HPP

#include "Features.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <exip/procTypes.h>

namespace exi {

/// Defined as `char` most of the time.
using Char   = exip::CharType;
/// A non-owning span over an array of `Char`s.
using StrRef = std::basic_string_view<Char>;

using CBinaryBuffer = exip::BinaryBuffer;
using CQName  = exip::QName;
using CString = exip::String;

} // namespace exi

#endif // EXIP_BASIC_HPP
