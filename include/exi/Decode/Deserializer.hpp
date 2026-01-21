//===- exi/Decode/Deserializer.hpp -----------------------------------===//
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
/// This file implements the interface used to decode EXI.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/ArrayRef.hpp>
#include <core/Common/PointerIntPair.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/EventCodes.hpp>

#define DEBUG_TYPE "Deserializer"

namespace exi {

class QName {
  // FIXME: Use more compact representation.
  const char* URI = nullptr;
  const char* Name = nullptr;
  const char* Pfx = nullptr;
  u32 URISize = 0;
  u32 NameSize = 0;
  u64 PfxSizeOrID = 0;

  QName(StrRef URI, StrRef Name, usize ID) :
   URI(URI.data()), Name(Name.data()), Pfx(nullptr),
   URISize(URI.size()), NameSize(Name.size()), PfxSizeOrID(ID) {}

  QName(StrRef URI, StrRef Name, StrRef Pfx) :
   URI(URI.data()), Name(Name.data()), Pfx(Pfx.data()),
   URISize(URI.size()), NameSize(Name.size()), PfxSizeOrID(Pfx.size()) {}

public:
  static QName New(StrRef URI, StrRef Name, StrRef Pfx) {
    return QName(URI, Name, Pfx);
  }
  static QName Unbound(StrRef URI, StrRef Name, u64 ID) {
    return QName(URI, Name, ID);
  }

  StrRef uri() const { return {URI, URISize}; }
  StrRef name() const { return {Name, NameSize}; }
  StrRef pfx() const { return Pfx ? StrRef(Pfx, PfxSizeOrID) : "?"_str; }
  u64 id() const { return EXI_ALWAYS(!Pfx) ? PfxSizeOrID : kInvalidLNI; }
  bool hasPrefix() const { return Pfx; }
  bool hasID() const { return !Pfx; }
};

class Deserializer {
public:
  /// Start Document
  virtual ExiError SD() {
    LOG_EXTRA("Beginning decoding...");
    return ExiError::OK;
  }

  /// End Document
  virtual ExiError ED() {
    LOG_EXTRA("Completed decoding!");
    return ExiError::DONE;
  }

  /// Start Element
  virtual ExiError SE(QName Name) {
    return ExiError::OK;
  }

  /// End Element
  virtual ExiError EE(QName Name) {
    return ExiError::OK;
  }

  /// Attribute
  virtual ExiError AT(QName Name, StrRef Value) {
    return ExiError::OK;
  }

  // TODO: Make typed AT variants that forward to normal AT by default.

  /// Namespace Declaration
  virtual ExiError NS(StrRef URI, StrRef Prefix) {
    return ExiError::OK;
  }

  /// Namespace Declaration - Local
  virtual ExiError NS_Local(StrRef URI, StrRef Prefix, u64 ID) {
    return this->NS(URI, Prefix);
  }

  /// Characters
  virtual ExiError CH(StrRef Value) {
    return ExiError::OK;
  }

  /// Comment
  virtual ExiError CM(StrRef Comment) {
    LOG_EXTRA("Decoded CM");
    return ExiError::OK;
  }

  /// Processing Instruction
  virtual ExiError PI(StrRef Target, StrRef Text) {
    LOG_EXTRA("Decoded PI");
    return ExiError::OK;
  }

  /// DOCTYPE (Full Text)
  virtual ExiError DT(StrRef FullText) {
    LOG_EXTRA("Decoded DT");
    return ExiError::OK;
  }

  /// DOCTYPE
  virtual ExiError DT(StrRef Name, StrRef PublicID,
                      StrRef SystemID, StrRef Text) {
    LOG_EXTRA("Decoded DT");
    return ExiError::OK;
  }
  
  /// Entity Reference
  virtual ExiError ER(StrRef Name) {
    LOG_EXTRA("Decoded ER");
    return ExiError::OK;
  }

  /// Self-Contained
  virtual ExiError SC() {
    LOG_EXTRA("Decoded SC");
    return ExiError::TODO;
  }

  /// Customization point for enabling or disabling persistence for uncommon
  /// values. Enable if strings are saved beyond the lifetime of the function.
  virtual bool needsPersistence() const { return false; }

  /// Customization point for simplifying DOCTYPE passing.
  virtual bool simpleDoctype() const { return false; }

  /// Destructor.
  virtual ~Deserializer() = default;

private:
  virtual void anchor();
};

} // namespace exi

#undef DEBUG_TYPE
