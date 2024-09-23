//===- exicpp/Content.hpp -------------------------------------------===//
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

#ifndef EXIP_CONTENT_HPP
#define EXIP_CONTENT_HPP

#include <type_traits>
#include <utility>
#include <exip/contentHandler.h>

#include "Basic.hpp"
#include "Errors.hpp"
#include "Strings.hpp"

// #if !EXICPP_CONCEPTS

#define GEN_REQUIRES(fn, ...) \
template <typename Source, typename AppData, typename = void>     \
struct Func_##fn { static constexpr bool value = false; };        \
                                                                  \
template <typename Source, typename AppData>                      \
struct Func_##fn<Source, AppData, std::void_t<decltype(           \
  Source::fn(__VA_ARGS__)                                         \
)>> { static constexpr bool value = true; };

namespace exi {
namespace H {

#define Decl std::declval

// For handling the meta-data (document structure)
GEN_REQUIRES(startDocument, Decl<AppData*>())
GEN_REQUIRES(endDocument,   Decl<AppData*>())
GEN_REQUIRES(startElement,  Decl<QName&>(), Decl<AppData*>())
GEN_REQUIRES(endElement,    Decl<AppData*>())
GEN_REQUIRES(attribute,     Decl<QName&>(), Decl<AppData*>())

// For handling the data
/*
errorCode (*intData)(Integer int_val, void* app_data);
errorCode (*booleanData)(boolean bool_val, void* app_data);
errorCode (*stringData)(const String str_val, void* app_data);
errorCode (*floatData)(Float float_val, void* app_data);
errorCode (*binaryData)(const char* binary_val, Index nbytes, void* app_data);
errorCode (*dateTimeData)(EXIPDateTime dt_val, void* app_data);
errorCode (*decimalData)(Decimal dec_val, void* app_data);
errorCode (*listData)(EXITypeClass exiType, unsigned int itemCount, void* app_data);
errorCode (*qnameData)(const QName qname, void* app_data); // xsi:type value only
*/

// Miscellaneous
/*
errorCode (*processingInstruction)(void* app_data); // TODO: define the parameters!
errorCode (*namespaceDeclaration)(const String ns, const String prefix, boolean isLocalElementNS, void* app_data);
*/

// For error handling
/*
errorCode (*warning)(const errorCode code, const char* msg, void* app_data);
errorCode (*error)(const errorCode code, const char* msg, void* app_data);
errorCode (*fatalError)(const errorCode code, const char* msg, void* app_data);
*/

#undef Decl

} // namespace H
} // namespace exi

#undef GEN_REQUIRES

// #endif

//======================================================================//
// Implementation
//======================================================================//

#if EXICPP_CONCEPTS
# include <concepts>
/// Assumes `Source` is defined in the current context.
# define REQUIRES_FUNC(fn, fn_args, call_args) if constexpr \
  (requires fn_args { \
    {Source::fn call_args} -> std::same_as<ErrCode>; \
  })
#else // !EXICPP_CONCEPTS
/// Assumes `Source` and `AppData` are defined in the current context.
# define REQUIRES_FUNC(fn, fn_args, call_args) if constexpr \
  (::exi::H::Func_##fn<Source, AppData>::value)
#endif

namespace exi {



} // namespace exi

#endif // EXIP_CONTENT_HPP
