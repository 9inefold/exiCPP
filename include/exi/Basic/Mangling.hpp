//===- exi/Basic/Mangling.hpp ---------------------------------------===//
//
// Copyright (C) 2025 Ninefold
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
/// This file defines the mangling functions for ExiHeader and ExiOptions.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Any.hpp>
#include <core/Common/Box.hpp>
#include <core/Common/MaybeBox.hpp>
#include <core/Common/D/Str.hpp>
#include <core/Common/EnumTraits.hpp>
#include <core/Common/Fundamental.hpp>
#include <core/Common/Option.hpp>
#include <core/Common/StringMap.hpp>
#include <exi/Basic/Bounded.hpp>

namespace exi {

// The current format is as such:
// A name such as `Member?` can be ommitted.
//
// ExiHeader {
//  HasCookie?:
//   C: yes
// 
//  ExiVersion:
//   0<N>: preview
//       : version 1 (ommitted)
//    <N>: version N
// 
//  HasOptions:
//   O<...>: yes
//   N: no
// }
//
// ExiOptions {
//  Alignment:
//   i: bIt
//   y: bYte
//   p: Precompression
//   c: Compression (implies p)
// 
//  Strict?:
//   S: true
// 
//  SelfContained?:
//   C: true
// 
//  Preserve:
//   P, then:
//    c: Comments
//    d: DTDs
//    l: LexicalValues
//    i: PIs
//    p: Prefixes
// 
//  SchemaID?:
//   Y<N><ID>: exists
// 
//  BlockSize?:
//   B<N>: size
// 
//  ValueMaxLength?:
//   If bounded: 
//    M<N>: length (M if compressed)
// 
//  ValuePartitionCapacity?:
//   If bounded: 
//    P<N>: length (P if compressed or has max length)
// }

struct ExiHeader;
struct ExiOptions;

//////////////////////////////////////////////////////////////////////////

/// Produces a unique string from the header values.
String exi_mangle_header(const ExiHeader& Header);

/// Produces header values from a mangled string.
/// @returns `true` on success.
bool exi_demangle_header(ExiHeader& Header, StrRef Sym);

//////////////////////////////////////////////////////////////////////////

/// Produces a unique string from the option values.
String exi_mangle_options(const ExiOptions& Opts);

/// Produces option values from a mangled string.
/// @returns `true` on success.
bool exi_demangle_options(ExiOptions& Opts, StrRef Sym);

} // namespace exi
