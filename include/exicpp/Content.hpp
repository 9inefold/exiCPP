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
#include <exip/contentHandler.h>

#include "Traits.hpp"
#include "Errors.hpp"
#include "Strings.hpp"

#ifdef __has_include
# if __has_include(<span>)
#  include <span>
# endif
#endif

namespace exi {
namespace H {

// TODO: Update
using Float   = exip::Float;
using Span    = StrRef;
using Decimal = Float;

#if !EXICPP_CONCEPTS

#define GEN_REQUIRES(fn, ...) \
template <typename Source, typename = void>                       \
struct Func_##fn { static constexpr bool value = false; };        \
                                                                  \
template <class Source>                                           \
struct Func_##fn<Source, std::void_t<decltype(                    \
  Decl<Source*>()->fn(__VA_ARGS__)                                \
)>> { static constexpr bool value = true; };

// For handling the meta-data (document structure)
GEN_REQUIRES(startDocument)
GEN_REQUIRES(endDocument)
GEN_REQUIRES(startElement,  Decl<QName&>())
GEN_REQUIRES(endElement)
GEN_REQUIRES(attribute,     Decl<QName&>())

// For handling the data
GEN_REQUIRES(intData,       std::uint64_t(0))
GEN_REQUIRES(booleanData,   false)
GEN_REQUIRES(stringData,    Decl<StrRef>())
GEN_REQUIRES(floatData,     Decl<Float>())
GEN_REQUIRES(binaryData,    Decl<Span>())
// GEN_REQUIRES(dateTimeData,  Decl<...>())
GEN_REQUIRES(decimalData,   Decl<Decimal>())
// GEN_REQUIRES(listData,      Decl<...>())
GEN_REQUIRES(qnameData,     Decl<QName&>())

// Miscellaneous
GEN_REQUIRES(processingInstruction)
GEN_REQUIRES(namespaceDeclaration,  
  Decl<StrRef>(), Decl<StrRef>(), false)

// For error handling
GEN_REQUIRES(warning,     ErrCode::Ok, Decl<StrRef>())
GEN_REQUIRES(error,       ErrCode::Ok, Decl<StrRef>())
GEN_REQUIRES(fatalError,  ErrCode::Ok, Decl<StrRef>())

// EXI specific
GEN_REQUIRES(selfContained)

#undef GEN_REQUIRES
#endif

} // namespace H
} // namespace exi

//======================================================================//
// Implementation
//======================================================================//

#if EXICPP_CONCEPTS
# include <concepts>
/// Assumes `Source` is defined in the current context.
# define REQUIRES_FUNC(fn, fn_args, call_args) if constexpr \
  (requires fn_args {                                       \
    requires requires(Source* src) {                        \
      {src->fn call_args} -> std::same_as<ErrCode>;         \
    };                                                      \
  })
#else // !EXICPP_CONCEPTS
/// Assumes `Source` is defined in the current context.
# define REQUIRES_FUNC(fn, fn_args, call_args) if constexpr \
  (::exi::H::Func_##fn<Source>::value)
#endif

#define CH_INIT_FUNC(fn, fn_args, call_args)  \
REQUIRES_FUNC(fn, fn_args, call_args) {       \
  handler.fn = &fn<Source>;                   \
}

namespace exi {

using CContentHandler = exip::ContentHandler;

class ContentHandler {
  template <class Source>
  static CErrCode startDocument(void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->startDocument();
    return CErrCode(ret);
  }

  template <class Source>
  static CErrCode endDocument(void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->endDocument();
    return CErrCode(ret);
  }

  template <class Source>
  static CErrCode startElement(CQName qname, void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->startElement(QName(qname));
    return CErrCode(ret);
  }

  template <class Source>
  static CErrCode endElement(void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->endElement();
    return CErrCode(ret);
  }

  template <class Source>
  static CErrCode attribute(CQName qname, void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->attribute(QName(qname));
    return CErrCode(ret);
  }

  ////////////////////////////////////////////////////////////////////////

  template <class Source>
  static CErrCode intData(exip::Integer val, void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->intData(val);
    return CErrCode(ret);
  }

  template <class Source>
  static CErrCode booleanData(exip::boolean val, void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->booleanData(
      static_cast<bool>(val)
    );
    return CErrCode(ret);
  }

  template <class Source>
  static CErrCode stringData(const CString val, void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->stringData(
      StrRef(val.str, val.length)
    );
    return CErrCode(ret);
  }

  template <class Source>
  static CErrCode binaryData(const char* val, Index nbytes, void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->binaryData(
      H::Span(val, nbytes)
    );
    return CErrCode(ret);
  }

  template <class Source>
  static CErrCode decimalData(exip::Decimal val, void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->decimalData(H::Decimal(val));
    return CErrCode(ret);
  }

  template <class Source>
  static CErrCode qnameData(CQName val, void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->qnameData(QName(val));
    return CErrCode(ret);
  }

  ////////////////////////////////////////////////////////////////////////

  template <class Source>
  static CErrCode processingInstruction(void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->processingInstruction();
    return CErrCode(ret);
  }

  template <class Source>
  static CErrCode namespaceDeclaration(
    const CString ns, const CString prefix,
    exip::boolean isLocalElement, 
    void* appData)
  {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->namespaceDeclaration(
      StrRef(ns.str, ns.length),
      StrRef(prefix.str, prefix.length),
      static_cast<bool>(isLocalElement)
    );
    return CErrCode(ret);
  }

  ////////////////////////////////////////////////////////////////////////

  template <class Source>
  static CErrCode warning(CErrCode err, const char* msg, void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->warning(
      ErrCode(err), StrRef(msg)
    );
    return CErrCode(ret);
  }

  template <class Source>
  static CErrCode error(CErrCode err, const char* msg, void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->error(
      ErrCode(err), StrRef(msg)
    );
    return CErrCode(ret);
  }

  template <class Source>
  static CErrCode fatalError(CErrCode err, const char* msg, void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->fatalError(
      ErrCode(err), StrRef(msg)
    );
    return CErrCode(ret);
  }

  ////////////////////////////////////////////////////////////////////////

  template <class Source>
  static CErrCode selfContained(void* appData) {
    const auto ptr = static_cast<Source*>(appData);
    const ErrCode ret = ptr->selfContained();
    return CErrCode(ret);
  }

public:
  template <class Source>
  static void SetContent(CContentHandler& handler, Source* data) {
    // For handling the meta-data (document structure)
    CH_INIT_FUNC(startDocument, (), ())
    CH_INIT_FUNC(endDocument,   (), ())
    CH_INIT_FUNC(startElement,  (QName qname), (qname))
    CH_INIT_FUNC(endElement,    (), ())
    CH_INIT_FUNC(attribute,     (QName qname), (qname))

    // For handling the data
    CH_INIT_FUNC(intData,     (std::uint64_t val), (val))
    CH_INIT_FUNC(booleanData, (bool val), (val))
    CH_INIT_FUNC(stringData,  (StrRef val), (val))
    CH_INIT_FUNC(binaryData,  (H::Span val), (val))
    // TODO: dateTimeData
    CH_INIT_FUNC(decimalData, (H::Decimal val), (val))
    // TODO: listData
    CH_INIT_FUNC(qnameData,   (QName val), (val))

    // Miscellaneous
    CH_INIT_FUNC(processingInstruction, (), (data))
    CH_INIT_FUNC(namespaceDeclaration,
      (StrRef ns, StrRef prefix, bool isLocalElement),
     (ns, prefix, isLocalElement))

    // For error handling
    CH_INIT_FUNC(warning,     (ErrCode err, StrRef msg), (err, msg))
    CH_INIT_FUNC(error,       (ErrCode err, StrRef msg), (err, msg))
    CH_INIT_FUNC(fatalError,  (ErrCode err, StrRef msg), (err, msg))

    // EXI specific
    CH_INIT_FUNC(selfContained, (), (data))
  }
};

} // namespace exi

#undef CH_INIT_FUNC

#endif // EXIP_CONTENT_HPP
