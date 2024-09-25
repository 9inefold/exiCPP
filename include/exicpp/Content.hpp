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
template <typename Source, typename AppData, typename = void>     \
struct Func_##fn { static constexpr bool value = false; };        \
                                                                  \
template <typename Source, typename AppData>                      \
struct Func_##fn<Source, AppData, std::void_t<decltype(           \
  Source::fn(__VA_ARGS__)                                         \
)>> { static constexpr bool value = true; };

#define Decl std::declval

// For handling the meta-data (document structure)
GEN_REQUIRES(startDocument, Decl<AppData*>())
GEN_REQUIRES(endDocument,   Decl<AppData*>())
GEN_REQUIRES(startElement,  Decl<QName&>(), Decl<AppData*>())
GEN_REQUIRES(endElement,    Decl<AppData*>())
GEN_REQUIRES(attribute,     Decl<QName&>(), Decl<AppData*>())

// For handling the data
GEN_REQUIRES(intData,       std::uint64_t(0), Decl<AppData*>())
GEN_REQUIRES(booleanData,   false,            Decl<AppData*>())
GEN_REQUIRES(stringData,    Decl<StrRef>(),   Decl<AppData*>())
GEN_REQUIRES(floatData,     Decl<Float>(),    Decl<AppData*>())
GEN_REQUIRES(binaryData,    Decl<Span>(),     Decl<AppData*>())
// GEN_REQUIRES(dateTimeData,  Decl<...>(),     Decl<AppData*>())
GEN_REQUIRES(decimalData,   Decl<Decimal>(),  Decl<AppData*>())
// GEN_REQUIRES(listData,      Decl<...>(),     Decl<AppData*>())
GEN_REQUIRES(qnameData,     Decl<QName&>(),   Decl<AppData*>())

// Miscellaneous
GEN_REQUIRES(processingInstruction, Decl<AppData*>())
GEN_REQUIRES(namespaceDeclaration,  
  Decl<StrRef>(), Decl<StrRef>(), false, Decl<AppData*>())

// For error handling
GEN_REQUIRES(warning,     ErrCode::Ok, Decl<StrRef>(), Decl<AppData*>())
GEN_REQUIRES(error,       ErrCode::Ok, Decl<StrRef>(), Decl<AppData*>())
GEN_REQUIRES(fatalError,  ErrCode::Ok, Decl<StrRef>(), Decl<AppData*>())

// EXI specific
GEN_REQUIRES(selfContained, Decl<AppData*>())

#undef Decl
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
  (requires fn_args { \
    {Source::fn call_args} -> std::same_as<ErrCode>; \
  })
#else // !EXICPP_CONCEPTS
/// Assumes `Source` and `AppData` are defined in the current context.
# define REQUIRES_FUNC(fn, fn_args, call_args) if constexpr \
  (::exi::H::Func_##fn<Source, AppData>::value)
#endif

namespace exi {

using CContentHandler = exip::ContentHandler;

class ContentHandler {
  template <typename Source, typename AppData>
  static CErrCode startDocument(void* appData) {
    const ErrCode ret = Source::startDocument(
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  template <typename Source, typename AppData>
  static CErrCode endDocument(void* appData) {
    const ErrCode ret = Source::endDocument(
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  template <typename Source, typename AppData>
  static CErrCode startElement(CQName qname, void* appData) {
    const ErrCode ret = Source::startElement(
      QName(qname),
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  template <typename Source, typename AppData>
  static CErrCode endElement(void* appData) {
    const ErrCode ret = Source::endElement(
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  template <typename Source, typename AppData>
  static CErrCode attribute(CQName qname, void* appData) {
    const ErrCode ret = Source::attribute(
      QName(qname),
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  ////////////////////////////////////////////////////////////////////////

  template <typename Source, typename AppData>
  static CErrCode intData(exip::Integer val, void* appData) {
    const ErrCode ret = Source::intData(
      val,
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  template <typename Source, typename AppData>
  static CErrCode booleanData(exip::boolean val, void* appData) {
    const ErrCode ret = Source::booleanData(
      static_cast<bool>(val),
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  template <typename Source, typename AppData>
  static CErrCode stringData(const CString val, void* appData) {
    const ErrCode ret = Source::stringData(
      StrRef(val.str, val.length),
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  template <typename Source, typename AppData>
  static CErrCode binaryData(const char* val, Index nbytes, void* appData) {
    const ErrCode ret = Source::binaryData(
      H::Span(val, nbytes),
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  template <typename Source, typename AppData>
  static CErrCode decimalData(exip::Decimal val, void* appData) {
    const ErrCode ret = Source::decimalData(
      H::Decimal(val),
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  template <typename Source, typename AppData>
  static CErrCode qnameData(CQName val, void* appData) {
    const ErrCode ret = Source::qnameData(
      QName(val),
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  ////////////////////////////////////////////////////////////////////////

  template <typename Source, typename AppData>
  static CErrCode processingInstruction(void* appData) {
    const ErrCode ret = Source::processingInstruction(
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  template <typename Source, typename AppData>
  static CErrCode namespaceDeclaration(
    const CString ns, const CString prefix,
    exip::boolean isLocalElement, 
    void* appData)
  {
    const ErrCode ret = Source::namespaceDeclaration(
      StrRef(ns.str, ns.length),
      StrRef(prefix.str, prefix.length),
      static_cast<bool>(isLocalElement),
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  ////////////////////////////////////////////////////////////////////////

  template <typename Source, typename AppData>
  static CErrCode warning(CErrCode err, const char* msg, void* appData) {
    const ErrCode ret = Source::warning(
      ErrCode(err), StrRef(msg),
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  template <typename Source, typename AppData>
  static CErrCode error(CErrCode err, const char* msg, void* appData) {
    const ErrCode ret = Source::error(
      ErrCode(err), StrRef(msg),
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  template <typename Source, typename AppData>
  static CErrCode fatalError(CErrCode err, const char* msg, void* appData) {
    const ErrCode ret = Source::fatalError(
      ErrCode(err), StrRef(msg),
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  ////////////////////////////////////////////////////////////////////////

  template <typename Source, typename AppData>
  static CErrCode selfContained(void* appData) {
    const ErrCode ret = Source::selfContained(
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

public:
  template <class Source, typename AppData>
  static void SetContent(CContentHandler& handler, AppData* data) {
    // For handling the meta-data (document structure)
    REQUIRES_FUNC(startDocument, (), (data)) {
      handler.startDocument = &startDocument<Source, AppData>;
    }
    REQUIRES_FUNC(endDocument, (), (data)) {
      handler.endDocument = &endDocument<Source, AppData>;
    }
    REQUIRES_FUNC(startElement, (QName qname), (qname, data)) {
      handler.startElement = &startElement<Source, AppData>;
    }
    REQUIRES_FUNC(endElement, (QName qname), (qname, data)) {
      handler.endElement = &endElement<Source, AppData>;
    }
    REQUIRES_FUNC(attribute, (QName qname), (qname, data)) {
      handler.attribute = &attribute<Source, AppData>;
    }

    // For handling the data
    REQUIRES_FUNC(intData, (std::uint64_t val), (val, data)) {
      handler.intData = &intData<Source, AppData>;
    }
    REQUIRES_FUNC(booleanData, (bool val), (val, data)) {
      handler.booleanData = &booleanData<Source, AppData>;
    }
    REQUIRES_FUNC(stringData, (StrRef val), (val, data)) {
      handler.stringData = &stringData<Source, AppData>;
    }
    REQUIRES_FUNC(binaryData, (H::Span val), (val, data)) {
      handler.binaryData = &binaryData<Source, AppData>;
    }
    // TODO: dateTimeData
    REQUIRES_FUNC(decimalData, (H::Decimal val), (val, data)) {
      handler.decimalData = &decimalData<Source, AppData>;
    }
    // TODO: listData
    REQUIRES_FUNC(qnameData, (QName val), (val, data)) {
      handler.qnameData = &qnameData<Source, AppData>;
    }

    // Miscellaneous
    REQUIRES_FUNC(processingInstruction, (), (data)) {
      handler.processingInstruction = &processingInstruction<Source, AppData>;
    }
    REQUIRES_FUNC(namespaceDeclaration,
      (StrRef ns, StrRef prefix, bool isLocalElement),
     (ns, prefix, isLocalElement, data)) {
      handler.namespaceDeclaration = &namespaceDeclaration<Source, AppData>;
    }

    // For error handling
    REQUIRES_FUNC(warning, (ErrCode err, StrRef msg), (err, msg, data)) {
      handler.warning = &warning<Source, AppData>;
    }
    REQUIRES_FUNC(error, (ErrCode err, StrRef msg), (err, msg, data)) {
      handler.error = &error<Source, AppData>;
    }
    REQUIRES_FUNC(fatalError, (ErrCode err, StrRef msg), (err, msg, data)) {
      handler.fatalError = &fatalError<Source, AppData>;
    }

    // EXI specific
    REQUIRES_FUNC(selfContained, (), (data)) {
      handler.selfContained = &selfContained<Source, AppData>;
    }
  }
};

} // namespace exi

#endif // EXIP_CONTENT_HPP
