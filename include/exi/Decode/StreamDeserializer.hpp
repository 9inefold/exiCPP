//===- exi/Decode/StreamDeserializer.hpp -----------------------------===//
//
// Copyright (C) 2026 Ninefold
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
/// This file implements the interface used to decode EXI to a raw_ostream.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/StringExtras.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/Except.hpp>
#include <exi/Decode/Deserializer.hpp>

namespace exi {

/// Writes information to an output stream.
class StreamDeserializer final : public Deserializer {
  raw_ostream& OS;
  exi::indent Indent;
  u64 UnboundURI = kInvalidPrefix;

public:
  StreamDeserializer(raw_ostream& OS, unsigned Scale = 2)
   : OS(OS), Indent(0, Scale) {}
  
  StreamDeserializer(raw_ostream& OS, exi::indent Indent)
   : OS(OS), Indent(Indent) {}

  /// Start Document
  ExiError SD() override {
    start() << "SD\n";
    ++Indent;
    return ExiError::OK;
  }

  /// End Document
  ExiError ED() override {
    --Indent;
    start() << "ED\n";
    return ExiError::DONE;
  }

  /// Start Element
  ExiError SE(QName Name) override {
    UnboundURI = Name.id();
    start() << "SE: ";
    this->write(Name) << '\n';
    ++Indent;
    return ExiError::OK;
  }

  /// End Element
  ExiError EE(QName Name) override {
    --Indent;
    start() << "EE: ";
    this->write(Name) << '\n';
    return ExiError::OK;
  }

  /// Attribute
  ExiError AT(QName Name, StrRef Value) override {
    start_half() << "AT: ";
    this->write(Name) << "=\"" << Value << "\"\n";
    return ExiError::OK;
  }

  ExiError NS_Full(StrRef URI, StrRef Prefix, bool Local) {
    start_half() << (Local ? "AT(local): "_str : "AT: "_str);
    OS << (Prefix.empty() ? "xmlns" : "xmlns:")
       << Prefix << "=\"" << URI << "\"\n";
    return ExiError::OK;
  }

  /// Namespace Declaration
  ExiError NS(StrRef URI, StrRef Prefix) override {
    return this->NS_Full(URI, Prefix, /*Local=*/false);
  }

  /// Namespace Declaration - Local
  ExiError NS_Local(StrRef URI, StrRef Prefix, u64 ID) override {
    if EXI_NEVER(!hasUnboundPrefix())
      Throw<argument_error>("local-name-ns set without valid SE!");
    if EXI_UNLIKELY(UnboundURI != ID)
      Throw<argument_error>("local-name-ns does not match SE URI!");
    // TODO: Verify this is correct?
    UnboundURI = kInvalidPrefix;
    return this->NS_Full(URI, Prefix, /*Local=*/true);
  }

  /// Characters
  ExiError CH(StrRef Value) override {
    start() << "CH: \"" << escape::cstylenq(Value) << "\"\n";
    return ExiError::OK;
  }

  /// Comment
  ExiError CM(StrRef Comment) override {
    start() << "CM: \"" << escape::cstyle(Comment) << "\"\n";
    return ExiError::OK;
  }

  /// Processing Instruction
  ExiError PI(StrRef Target, StrRef Text) override {
    start() << "PI: " << Target << ' ' << escape::cstylenq(Text) << '\n';
    return ExiError::OK;
  }

  /// DOCTYPE
  ExiError DT(StrRef FullText) override {
    start() << "DT: " << escape::cstylenq(FullText) << '\n';
    return ExiError::OK;
  }
  
  /// TODO: Entity Reference
  ExiError ER(StrRef Name) override {
    start() << "ER: &" << Name << ";\n";
    return ExiError::OK;
  }

  raw_ostream& os() { return OS; }
  bool needsPersistence() const override { return false; }
  bool simpleDoctype() const override { return true; }

private:
  bool hasUnboundPrefix() const { return UnboundURI != kInvalidLNI; }
  raw_ostream& start() { return OS << Indent; }
  raw_ostream& start_half() {
    bool Extra = (Indent.Scale % 2 != 0);
    OS << (Indent - 1);
    return OS.indent((Indent.Scale / 2) + Extra);
  }

  raw_ostream& write(const QName& Name) {
    if (Name.hasPrefix())
      if (StrRef Pfx = Name.pfx(); !Pfx.empty())
        OS << Pfx << ':';
    return OS << Name.name();
  }

  void anchor() override;
};

} // namespace exi
