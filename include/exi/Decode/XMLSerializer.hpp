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

#include <core/Common/Twine.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/XMLContainer.hpp>
#include <exi/Decode/Serializer.hpp>

namespace exi {

// TODO: Definitely wanna do some caching here...
class XMLSerializer : public Serializer {
  mutable XMLDocument Doc;
  XMLNode* Curr = nullptr;
  XMLAttribute* Attr = nullptr;

public:
  using enum xml::NodeKind;

  XMLSerializer() : Doc() {}

  /// Start Document
  ExiError SD() override {
    this->Doc.clear();
    this->Curr = Doc.document();
    this->Attr = nullptr;
    return ExiError::OK;
  }

  /// End Document
  ExiError ED() override {
    this->Curr = Doc.document();
    this->Attr = nullptr;
    return ExiError::DONE;
  }

  /// Start Element
  ExiError SE(QName Name) override {
    XMLNode* Node = allocNode(node_element, Name);
    Curr->append_node(Node);
    Curr = Node;
    return ExiError::OK;
  }

  /// End Element
  ExiError EE(QName Name) override {
    Curr = Curr->parent();
    if EXI_UNLIKELY(!Curr)
      Curr = Doc.document();
    return ExiError::OK;
  }

  /// Attribute
  ExiError AT(QName Name, StrRef Value) override {
    this->Attr = allocAttr(Name, Value);
    Curr->append_attribute(Attr);
    return ExiError::OK;
  }

  /// Namespace Declaration
  ExiError NS(StrRef URI, StrRef Prefix, bool LocalElementNS) override {
    const QName Name { .Name = Prefix, .Prefix = "xmlns"_str };
    this->Attr = allocAttr(Name, URI);
    Curr->append_attribute(Attr);
    return ExiError::OK;
  }

  /// Namespace Declaration
  ExiError CH(StrRef Value) override {
    XMLNode* Node = allocValue(node_data, Value);
    // Curr->value(Value);
    Curr->prepend_node(Node);
    return ExiError::OK;
  }

  /// Comment
  ExiError CM(StrRef Comment) override {
    XMLNode* Node = allocValue(node_comment, intern(Comment));
    Curr->append_node(Node);
    return ExiError::OK;
  }

  /// Processing Instruction
  ExiError PI(StrRef Target, StrRef Text) override {
    XMLNode* Node = allocNode(node_pi,
      intern(Target), intern(Text));
    Curr->append_node(Node);
    return ExiError::OK;
  }

  /// DOCTYPE
  ExiError DT(StrRef Name, StrRef PublicID,
              StrRef SystemID, StrRef Text) override {
    // TODO: Use other params
    XMLNode* Node = allocValue(node_doctype, intern(Text));
    Curr->append_node(Node);
    return ExiError::OK;
  }
  
  /// TODO: Entity Reference
  ExiError ER(StrRef Name) override {
    outs() << "ER: " << Name << '\n';
    return ExiError::OK;
  }

  XMLDocument& document() { return Doc; }

private:
  XMLNode* allocNode(NodeKind Kind, QName Name) {
    StrRef FullName = getFullName(Name);
    return Doc.allocate_node(Kind,
      FullName.data(), nullptr,
      FullName.size(), 0
    );
  }

  XMLNode* allocValue(NodeKind Kind, StrRef Value) {
    return Doc.allocate_node(Kind,
      nullptr, Value.data(),
            0, Value.size()
    );
  }
  
  ALWAYS_INLINE XMLNode* allocNode(NodeKind Kind, StrRef Name, StrRef Value) {
    return Doc.allocate_node(Kind, Name, Value);
  }

  ALWAYS_INLINE XMLAttribute* allocAttr(QName Name, StrRef Value) {
    StrRef FullName = getFullName(Name);
    return Doc.allocate_attribute(FullName, Value);
  }

  ALWAYS_INLINE XMLAttribute* allocAttr(StrRef Name, StrRef Value) {
    return Doc.allocate_attribute(Name, Value);
  }

  ALWAYS_INLINE XMLAttribute* allocAttr(StrRef Name) {
    return Doc.allocate_attribute(
      Name.data(), nullptr,
      Name.size(), 0
    );
  }

  EXI_INLINE StrRef getFullName(const QName& Name) {
    StrRef FullName = Name.getName();
    if (Name.hasPrefix()) {
      const StrRef Prefix = Name.getPrefix();
      return intern(Prefix + Twine(':') + FullName);
    }
    return FullName;
  }

  StrRef intern(const Twine& FullName) {
    SmallStr<32> Data;
    StrRef Val = FullName.toStrRef(Data);
    return this->intern(Val);
  }

  StrRef intern(StrRef FullName) {
    const usize Size = FullName.size();
    const char* Out = Doc.allocString(FullName.data(), Size);
    return {Out, Size};
  }
};

} // namespace exi
