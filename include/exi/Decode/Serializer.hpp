//===- exi/Decode/Serializer.hpp -------------------------------------===//
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
/// This file implements the interface used to decode EXI as XML.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/ArrayRef.hpp>
#include <core/Common/PointerIntPair.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/ErrorCodes.hpp>

#define DEBUG_TYPE "BodyDecoder"

namespace exi {

class QName {
public:
  // FIXME: Use more compact representation.
  StrRef URI = "";
  StrRef Name = "";
  StrRef Prefix = "";
public:
  StrRef getURI() const { return URI; }
  StrRef getName() const { return Name; }
  StrRef getPrefix() const { return Prefix; }
  bool hasPrefix() const { return !Prefix.empty(); }
};

class Serializer {
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

  /// Self-Contained
  virtual ExiError SC() {
    LOG_EXTRA("Decoded SC");
    return ExiError::OK;
  }

  /// Attribute
  virtual ExiError AT(QName Name, StrRef Value) {
    return ExiError::OK;
  }

  // TODO: Make typed AT variants that forward to normal AT by default.

  /// Namespace Declaration
  // TODO: Fix signature, non-local NS will refer back to another prefix.
  virtual ExiError NS(StrRef URI, StrRef Prefix, bool LocalElementNS) {
    return ExiError::OK;
  }

  /// Namespace Declaration
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

  /// Customization point for enabling or disabling persistence for uncommon
  /// values. Enable if strings are saved beyond the lifetime of the function.
  virtual bool needsPersistence() const { return false; }

  virtual ~Serializer() = default;

private:
  virtual void anchor();
};

} // namespace exi

#undef DEBUG_TYPE
