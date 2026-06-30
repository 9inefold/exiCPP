//===- exi/Decode/XMLDeserializer.hpp --------------------------------===//
//
// Copyright (C) 2025-2026 Ninefold
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

#include <core/Common/Box.hpp>
#include <core/Common/DenseMap.hpp>
#include <core/Common/Twine.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/Except.hpp>
#include <exi/Basic/XMLContainer.hpp>
#include <exi/Decode/Deserializer.hpp>

#define DEBUG_TYPE "XMLDeserializer"

namespace exi {

// TODO: Definitely wanna do some caching here...
class XMLDeserializer final : public Deserializer, public XMLCoderOptions {
  using enum xml::NodeKind;
  using enum UnboundURIKind;
  using enum PreserveCDATAKind;
  using NSSlotMap = SmallDenseMap<u64, u64>;

  mutable Option<XMLDocument> SelfDoc;
  XMLDocument* Doc = nullptr;
  XMLNode* Curr = nullptr;
  XMLAttribute* Attr = nullptr;
  u64 UnboundURI = kInvalidPrefix;
  StrRef CurrURI = "";
  MiBox<NSSlotMap> NSSlots;
  SmallStr<32> ScratchBuf;
  // For exificient compat
  bool HasPrefix = true;

public:
  XMLDeserializer(XMLDocument& Doc) : Doc(&Doc), Curr(Doc.document()) {}
  XMLDeserializer() {
    Doc = &SelfDoc.emplace();
    Curr = Doc->document();
  }

  XMLDeserializer(const XMLDeserializer&) = delete;
  XMLDeserializer(XMLDeserializer&&) = delete;
  XMLDeserializer& operator=(const XMLDeserializer&) = delete;
  XMLDeserializer& operator=(XMLDeserializer&&) = delete;

  /// Start Document
  ExiError SD() override {
    Doc->clear();
    this->Curr = Doc->document();
    this->Attr = nullptr;
    this->CurrURI = "";
    if (XMLCoderOptions::UURIType == UURI_CUSTOM) {
      LOG_WARN("UURI_CUSTOM is unsupported, setting to UURI_UNIVERSAL");
      XMLCoderOptions::UURIType = UURI_UNIVERSAL;
    } else if (XMLCoderOptions::UURIType != UURI_UNIVERSAL) {
      NSSlots = make_miunique<NSSlotMap>();
    }
    return ExiError::OK;
  }

  /// End Document
  ExiError ED() override {
    this->Curr = Doc->document();
    this->Attr = nullptr;
    this->CurrURI = "";
    NSSlots.reset();
    return ExiError::DONE;
  }

  /// Start Element
  ExiError SE(QName Name) override {
    this->setURIPrefixForUnboundSE();
    XMLNode* Node = allocNode(node_element, Name);
    Curr->append_node(Node);
    this->UnboundURI = Name.id();
    this->HasPrefix = Name.hasPrefix();
    if (!this->HasPrefix)
      this->CurrURI = Name.uri();
    Curr = Node;
    return ExiError::OK;
  }

  /// End Element
  ExiError EE(QName Name) override {
    this->setURIPrefixForUnboundSE();
    Curr = Curr->parent();
    if EXI_UNLIKELY(!Curr)
      Curr = Doc->document();
    return ExiError::OK;
  }

  /// Attribute
  ExiError AT(QName Name, StrRef Value) override {
    this->Attr = allocAttr</*IsNS=*/false>(Name, Value);
    Curr->append_attribute(Attr);
    return ExiError::OK;
  }

  /// xsi:type
  ExiError AT_XsiType(QName Name, QName Value) override {
    StrRef FullValue = this->getFullNameAT(Value, /*IsNS=*/false);
    this->Attr = allocAttr</*IsNS=*/false>(Name, FullValue);
    Curr->append_attribute(Attr);
    return ExiError::OK;
  }

  /// Namespace Declaration
  ExiError NS(StrRef URI, StrRef Prefix) override {
    const auto Name = QName::New(URI, Prefix, "xmlns"_str);
    this->Attr = allocAttr</*IsNS=*/true>(Name, URI);
    Curr->prepend_attribute(Attr);
    return ExiError::OK;
  }

  /// Namespace Declaration - Local
  ExiError NS_Local(StrRef URI, StrRef Prefix, u64 ID) override {
    if (this->HasPrefix) {
      this->HasPrefix = false;
      return this->NS(URI, Prefix);
    }
    if EXI_NEVER(!hasUnboundPrefix())
      Throw<argument_error>("local-name-ns set without valid SE!");
    if EXI_UNLIKELY(UnboundURI != ID)
      Throw<argument_error>("local-name-ns does not match SE URI!");
    // Last sanity check!
    exi_expensive_invariant(CurrURI == URI);
    // TODO: Verify this is correct?
    this->UnboundURI = kInvalidLNI;
    StrRef FullName = getFullName(Prefix, Curr->name());
    Curr->name(FullName);
    return this->NS(URI, Prefix);
  }

  /// Characters
  ExiError CH(StrRef Value) override {
    usize From = 0;
    if (XMLCoderOptions::SkipEmptyCH) {
      // Save result.
      From = Value.find_first_not_of(" \t\r\n");
      if (From == StrRef::npos)
        return ExiError::OK;
    }
    if (XMLCoderOptions::PreserveCDATA == CDATA_PRESERVE) {
      if (auto I = Value.find("<![CDATA[", From); I != StrRef::npos)
        return this->CH_CDATA(Value, I);
    }
    // No CDATA found.
    this->makeCHValue(false, Value);
    return ExiError::OK;
  }

  /// Characters + CDATA
  ExiError CH_CDATA(StrRef Value, const usize First) {
    constexpr auto CDATA_Start = "<![CDATA["_str;
    constexpr auto CDATA_End = "]]>"_str;
    static_assert(CDATA_Start.size() == 9);
    exi_assert(First != StrRef::npos);

    if (First) {
      XMLNode* Node = allocValue(
        node_data, Value.take_front(First));
      Curr->append_node(Node);
      Value = Value.drop_front(First);
    }
    
    while (!Value.empty()) {
      StrRef::size_type End = Value.find(
        CDATA_End, CDATA_Start.size());
      if EXI_UNLIKELY(End == StrRef::npos) {
        LOG_WARN("Unterminated CDATA block: {}", Value);
        return ErrorCode::kUnexpectedError;
      }
      
      this->makeCHValue(true, Value.substr(9, End - 9));
      Value = Value.drop_front(End + 3);

      StrRef::size_type Start = Value.find(CDATA_Start);
      if (Start == StrRef::npos) break;
      this->makeCHValue(false, Value.take_front(Start));
      Value = Value.drop_front(Start);
    }

    // No CDATA found.
    this->makeCHValue(false, Value);
    return ExiError::OK;
  }

  /// Comment
  ExiError CM(StrRef Comment) override {
    XMLNode* Node = allocValue(node_comment, Comment);
    Curr->append_node(Node);
    return ExiError::OK;
  }

  /// Processing Instruction
  ExiError PI(StrRef Target, StrRef Text) override {
    XMLNode* Node = allocNode(node_pi, Target, Text);
    Curr->append_node(Node);
    return ExiError::OK;
  }

  /// DOCTYPE
  ExiError DT(StrRef FullText) override {
    // TODO: Use other params
    XMLNode* Node = allocValue(node_doctype, FullText);
    Curr->append_node(Node);
    return ExiError::OK;
  }
  
  /// TODO: Entity Reference
  ExiError ER(StrRef Name) override {
    outs() << "ER: " << Name << '\n';
    return ExiError::OK;
  }

  XMLDocument& document() { exi_invariant(Doc); return *Doc; }
  bool needsPersistence() const override { return true; }
  bool simpleDoctype() const override { return true; }

private:
  bool hasUnboundPrefix() const {
    return Curr && UnboundURI != kInvalidLNI;
  }

  void makeCHValue(bool IsCDATA, StrRef Value) {
    if (!IsCDATA && Value.empty()) return;
    NodeKind Kind = IsCDATA ? node_cdata : node_data;
    XMLNode* Node = allocValue(Kind, Value);
    Curr->append_node(Node);
  }

  XMLNode* allocNode(NodeKind Kind, QName Name) {
    StrRef FullName = getFullName(Name);
    return Doc->allocate_node(Kind,
      FullName.data(), nullptr,
      FullName.size(), 0
    );
  }

  XMLNode* allocValue(NodeKind Kind, StrRef Value) {
    return Doc->allocate_node(Kind,
      nullptr, Value.data(),
            0, Value.size()
    );
  }
  
  ALWAYS_INLINE XMLNode* allocNode(NodeKind Kind, StrRef Name, StrRef Value) {
    return Doc->allocate_node(Kind, Name, Value);
  }

  template <bool IsNS>
  ALWAYS_INLINE XMLAttribute* allocAttr(QName Name, StrRef Value) {
    StrRef FullName = getFullNameAT(Name, IsNS);
    return Doc->allocate_attribute(FullName, Value);
  }

  ALWAYS_INLINE XMLAttribute* allocAttr(StrRef Name, StrRef Value) {
    return Doc->allocate_attribute(Name, Value);
  }

  ALWAYS_INLINE XMLAttribute* allocAttr(StrRef Name) {
    return Doc->allocate_attribute(
      Name.data(), nullptr,
      Name.size(), 0
    );
  }

  /// If there is an unbound prefix, and the URI is not empty, the name will be
  /// set to whatever the provided mode is.
  void setURIPrefixForUnboundSE();

  /// Creates a new unbound prefix given the type in `XMLCoderOptions`.
  StrRef getURIPrefixForUnbound(StrRef URI, StrRef LocalName, u64 ID);
  /// Creates a new unbound prefix given the type in `XMLCoderOptions`.
  EXI_INLINE StrRef getURIPrefixForUnbound(const QName& Name) {
    exi_invariant(Name.hasID());
    return getURIPrefixForUnbound(Name.uri(), Name.name(), Name.id());
  }

  /// Creates a name like `{URI}LocalName`.
  StrRef getUnboundPrefixUniversal(StrRef URI, StrRef LocalName);
  /// Creates a name like `ns3:LocalName`.
  StrRef getUnboundPrefixExificient(StrRef URI, StrRef LocalName, u64 ID);
  /// Creates a name like `p0:LocalName`.
  StrRef getUnboundPrefixOpenexi(StrRef URI, StrRef LocalName, u64 ID);

  StrRef getUnboundPrefixCommon(StrRef URI, StrRef LocalName, bool IsNew);
  std::pair<u64, bool> getNSSlot(u64 ID);

  EXI_INLINE StrRef getFullName(const QName& Name) {
    return getFullName(Name.pfx(), Name.name());
  }

  StrRef getFullName(StrRef Pfx, StrRef Name) {
    // TODO: Handle Name.empty()
    if (Name.empty()) {
      LOG_WARN("Empty name SE: {}:{}", Pfx, Name);
      return intern(Pfx, Name);
    }
    if (!Pfx.empty())
      return intern(Pfx, Name);
    return intern(Name);
  }

  StrRef getFullNameAT(const QName& Name, bool IsNS);

  StrRef intern(const Twine& FullName) {
    SmallStr<32> Buf;
    StrRef Val = FullName.toStrRef(Buf);
    return this->intern(Val);
  }

  inline StrRef intern(StrRef Prefix, StrRef Name) {
    return intern(Prefix + Twine(':') + Name);
  }

  StrRef intern(StrRef FullName) {
    const usize Size = FullName.size();
    const char* Out = Doc->allocString(FullName.data(), Size);
    return {Out, Size};
  }

  void anchor() override;
};

// TODO: Add InFlightXMLSerializer

} // namespace exi

#undef DEBUG_TYPE
