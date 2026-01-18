//===- exi/Encode/XMLSerializer.cpp ----------------------------------===//
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
/// This file implements the interface used to encode XML as EXI.
///
//===----------------------------------------------------------------===//

#include <exi/Encode/XMLSerializer.hpp>
#include <core/Common/EnumArray.hpp>
#include <core/Common/StringSwitch.hpp>
#include <core/Common/Unwrap.hpp>
#include <core/Common/bitset.hpp>
#include <core/Support/Alignment.hpp>
#include <core/Support/Casting.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/D/InternalMacros.hpp>
#include <exi/Encode/DepthValidator.hpp>
#include <exi/Encode/DTDParser.hpp>
#include <exi/Encode/Event.hpp>
//#include <Encode/ChannelEncoder-inl.hpp>
#include <Encode/OrderedEncoder-inl.hpp>

using namespace exi;
using namespace exi::encode;

#define DEBUG_TYPE "XMLSerializer"

/// Contains data on the kind of nodes allowed.
using NodeKindBitset = std::bitset<usize(NodeKind::node_last)>;
/// Contains: Document, Element, Data, and CDATA.
/// FIXME: Add option to ignore CDATA.
static constexpr NodeKindBitset kNKBitsetDefault = 0b0000'1110;
/// Contains: DOCTYPE, Comment, and Processing Instruction.
static constexpr NodeKindBitset kNKBitsetDocContent = 0b1101'0000;
/// Contains: Comment, and Processing Instruction.
static constexpr NodeKindBitset kNKBitsetDocEnd = 0b1001'0000;

//////////////////////////////////////////////////////////////////////////
// EncoderRunner

namespace INTERNAL_NS(exi) {

class INTERNAL_LINKAGE GenericXMLEncoderRunner {
protected:
  const NodeKindBitset AllowedKinds;
public:
  GenericXMLEncoderRunner(BodyEncoder& BE);
};

template <class Encoder>
class INTERNAL_LINKAGE XMLEncoderRunner;

template <>
class INTERNAL_LINKAGE XMLEncoderRunner<OrderedEncoder>
    : public GenericXMLEncoderRunner {
  using Base = GenericXMLEncoderRunner;
  using enum SimpleEventTerm;
  using Encoder = OrderedEncoder; // TODO: Remove
  Encoder& BE;
  encode::Schema* TheSchema = nullptr;
  NodeKindBitset CurrKinds;
  XMLSerializerOpts Opts;

public:
  XMLEncoderRunner(Encoder& BE, XMLSerializerOpts Opts)
   : GenericXMLEncoderRunner(BE),
      BE(BE), TheSchema(BE.getSchema()),
      CurrKinds(0b1), Opts(Opts) {
  }

  ALWAYS_INLINE void loadKinds() {
    this->CurrKinds = Base::AllowedKinds;
  }
  ALWAYS_INLINE void andKinds(const NodeKindBitset& Other) {
    this->CurrKinds = (Base::AllowedKinds & Other);
  }
  bool needsDocEndLoop() const {
    return Base::AllowedKinds[NodeKind::node_pi]
        || Base::AllowedKinds[NodeKind::node_comment];
  }

  // TODO: Add StackExhaustionHandler
  ExiError run(XMLDocument& Doc) {
    XMLNode* N = Doc.first_node();
    if EXI_UNLIKELY(!N) {
      LOG_ERROR("Document: Must have root node.");
      return ErrorCode::kUnexpectedError;
    }
  // Document:
    exi_try(BE.StartDocument());
  // DocContent:
    this->andKinds(kNKBitsetDocContent);
    while (N->type() != NodeKind::node_element) {
      exi_try(handleDocContent(*N));
      N = N->next_sibling();
      if EXI_UNLIKELY(!N) {
        LOG_ERROR("DocContent: Expected SE before ED.");
        return ErrorCode::kInconsistentProcState;
      }
    }

  // Content:
    this->loadKinds(/*Options*/);
    exi_try(handleTopLevelSE(N));

  // DocEnd:
    this->andKinds(kNKBitsetDocEnd);
    N = N->next_sibling();
    if (needsDocEndLoop()) while(N) {
      exi_try(handleDocEnd(*N));
      N = N->next_sibling();
    }
    return BE.EndDocument();
  }

private:
  static bool IsSENode(XMLNode* N) {
    if EXI_UNLIKELY(!N)
      return false;
    return N->type() == NodeKind::node_element;
  }

  static bool IsRootNode(XMLNode* N) {
    if (!IsSENode(N))
      return false;
    if (auto* P = N->parent())
      return P->type() == NodeKind::node_document;
    return true;
  }

  ////////////////////////////////////////////////////////////////////////
  // PreParse

  /// Caches information which can be used by the namespaces later.
  struct PPAttributeCtx {
    SmallVec<XMLAttribute*, 3> NS;
    XMLAttribute* XsiType = nullptr;
    XMLAttribute* XsiNil  = nullptr;
    SmallVec<XMLAttribute*, 4> Attrs;
    XMLAttribute* LocalNS = nullptr;
  };

  inline ExiError ppAddAttr(XMLAttribute* const A, const StrRef&,
                            PPAttributeCtx& Ctx) {
    Ctx.Attrs.push_back(A);
    return ExiError::OK;
  }

  template <xml::IdentifierKind IK>
  ExiError ppAddXsi(XMLAttribute* const A, const StrRef&,
                    PPAttributeCtx& Ctx) {
    static constexpr bool kIsNil = (IK == xml::IK_XsiNil);
    static_assert(kIsNil || IK == xml::IK_XsiType);
    exi_invariant(A && A->id_kind() == xml::IK_XsiNil);
    static constexpr auto XsiV = kIsNil 
      ? &PPAttributeCtx::XsiNil
      : &PPAttributeCtx::XsiType;
    if EXI_UNLIKELY(Ctx.*XsiV) {
      LOG_ERROR("Multiple {} in attribute list!",
                kIsNil ? "xsi:nil" : "xsi:type");
      return ErrorCode::kInvalidEXIInput;
    }
    Ctx.*XsiV = A;
    return ExiError::OK;
  }

  template <bool IsEmpty>
  inline ExiError ppAddNamespace(XMLAttribute* const A, const StrRef& Pfx,
                                 PPAttributeCtx& Ctx) {
    if constexpr (IsEmpty) {
      if (Pfx.empty())
        Ctx.LocalNS = A;
    } else {
      auto AttrName = A->name().drop_front(6);
      if (Pfx == AttrName)
        Ctx.LocalNS = A;
    }
    Ctx.NS.push_back(A);
    return ExiError::OK;
  }

  EXI_INLINE ExiError ppDispatchNodeAttr(XMLAttribute* const A,
                                         const StrRef& Pfx,
                                         PPAttributeCtx& Ctx) {
    switch (A->id_kind()) {
    case xml::IK_Name:
      tail_return this->ppAddAttr(A, Pfx, Ctx);
    case xml::IK_XsiNil:
      tail_return this->ppAddXsi<xml::IK_XsiNil>(A, Pfx, Ctx);
    case xml::IK_XsiType:
      tail_return this->ppAddXsi<xml::IK_XsiType>(A, Pfx, Ctx);
    case xml::IK_AnonNS:
      tail_return this->ppAddNamespace<true>(A, Pfx, Ctx);
    case xml::IK_NamedNS:
      tail_return this->ppAddNamespace<false>(A, Pfx, Ctx);
    case xml::IK_None:
      exi_guardrail("empty attributes are not valid");
    }
    exi_guardrail("invalid node type");
  }

  /// Handles parsing out attributes and such.
  ExiError preparseNodeAttrs(XMLNode* const N, PPAttributeCtx& Ctx) {
    XMLAttribute* A = N->first_attribute();
    exi_assume(A != nullptr);
    auto [Pfx, _] = BodyEncoder::SplitName(N->name());
    while (A) {
      if (auto E = ppDispatchNodeAttr(A, Pfx, Ctx))
        return E;
      A = A->next_attribute();
    }
    return ExiError::OK;
  }

  ////////////////////////////////////////////////////////////////////////
  // SE + NS

  EXI_INLINE static StrRef GetPrefixFromNS(XMLAttribute* A) {
    exi_invariant(A->is_namespace(), "AT or xsi:* passed to NS handler.");
    const bool IsAnon = (A->id_kind() == xml::IK_AnonNS);
#if EXI_DEBUG
    StrRef Out = A->name();
    if (IsAnon) {
      exi_assert(Out.consume_front("xmlns"));
      return ""_str;
    } else {
      exi_assert(Out.consume_front("xmlns:"));
      return Out;
    }
#else
    // If anon, must be empty. Otherwise, drop 'xmlns:` and return.
    return IsAnon ? ""_str : A->name().drop_front(6);
#endif
  }

  static NamespaceEvent MakeNSEvent(XMLAttribute* NS) {
    return make_event<SimpleEventTerm::NS>(
      GetPrefixFromNS(NS), NS->value());
  }
  static NamespaceEvent MakeNSEvent(XMLAttribute* NS, XMLAttribute* LocalNS) {
    return make_event<SimpleEventTerm::NS>(
      GetPrefixFromNS(NS), NS->value(), NS == LocalNS);
  }

  static AttrEvent MakeATEvent(XMLAttribute* AT) {
    exi_invariant(AT->is_name(), "NS or xsi:* passed to AT handler.");
    return make_event<SimpleEventTerm::AT>(AT->name(), AT->value());
  }

  /// Handles StartElement with a local-element-ns.
  ExiError handleSEWithLocalNS(XMLNode* N, PPAttributeCtx& Ctx) {
    exi_invariant(Ctx.LocalNS, "local-element-ns cannot be null");
    return BE.StartElementURI(N->name(), Ctx.LocalNS->value());
  }

  /// Handles StartElement in different ways depending on if a local-element-ns
  /// was encountered.
  ExiError dispatchHandleSE(XMLNode* N, PPAttributeCtx& Ctx) {
    if EXI_LIKELY(!Ctx.LocalNS)
      return BE.StartElement(N->name());
    tail_return this->handleSEWithLocalNS(N, Ctx);
  }

  /// Simple case, no local-element-ns.
  template <bool IsRoot>
  ExiError handleNS(XMLNode*, PPAttributeCtx& Ctx) {
    SmallVec<NamespaceEvent> NSBatch;
    NSBatch.reserve(Ctx.NS.size());
    for (XMLAttribute* NS : Ctx.NS)
      NSBatch.push_back(MakeNSEvent(NS));
    return BE.BatchNamespace<IsRoot>(NSBatch);
  }

  /// Complex case, a local-element-ns was encountered.
  template <bool IsRoot>
  ExiError handleNSWithLocalNS(XMLNode*, PPAttributeCtx& Ctx) {
    SmallVec<NamespaceEvent> NSBatch;
    NSBatch.reserve(Ctx.NS.size());
    for (XMLAttribute* NS : Ctx.NS)
      NSBatch.push_back(MakeNSEvent(NS, Ctx.LocalNS));
    return BE.BatchNamespace<IsRoot>(NSBatch);
  }

  /// Handles NameSpace in different ways depending on if a local-element-ns
  /// was encountered.
  template <bool IsRoot>
  ExiError dispatchHandleNS(XMLNode* N, PPAttributeCtx& Ctx) {
    if (!Ctx.LocalNS)
      tail_return this->handleNS<IsRoot>(N, Ctx);
    else
      tail_return this->handleNSWithLocalNS<IsRoot>(N, Ctx);
  }

  /// Handles the `xsi:{type, nil}` ATtributes, if required.
  ExiError handleXsiBuiltins(XMLNode*, PPAttributeCtx& Ctx) {
    if EXI_NEVER(Ctx.XsiNil || Ctx.XsiType) {
      LOG_ERROR("'xsi:*' builtins are currently unimplemented!");
      return ExiError::TODO;
    }
    return ExiError::OK;
  }

  /// Handles ATtributes. Assumes unsorted.
  ExiError handleAttributes(XMLNode*, PPAttributeCtx& Ctx) {
    SmallVec<AttrEvent> ATBatch;
    ATBatch.reserve(Ctx.Attrs.size());
    for (XMLAttribute* AT : Ctx.Attrs)
      ATBatch.push_back(MakeATEvent(AT));
    return BE.BatchAttribute(ATBatch);
  }

  /// Handles StartElement events + PreParsed ATtributes.
  template <bool IsRoot = false>
  EXI_FLATTEN ExiError handleSEWithPPAT(XMLNode* N, PPAttributeCtx& Ctx) {
    LOG_EXTRA(">>> Scanned {} NS{}, {} AT",
              Ctx.NS.size(),
              Ctx.LocalNS ? " (local)" : "",
              Ctx.Attrs.size());
    exi_try(dispatchHandleSE(N, Ctx));
    exi_try(dispatchHandleNS<IsRoot>(N, Ctx));
    exi_try(handleXsiBuiltins(N, Ctx));
    tail_return handleAttributes(N, Ctx);
  }

  /// Handles StartElement and its ATtributes.
  template <bool IsRoot = false>
  ExiError handleSEAndAT(XMLNode* N) {
    if (N->first_attribute()) {
      PPAttributeCtx PPCtx;
      exi_try(preparseNodeAttrs(N, PPCtx));
      // Handle passing of the namespaces.
      return handleSEWithPPAT<IsRoot>(N, PPCtx);
    }
    /// Simple StartElement!
    return BE.StartElement(N->name());
  }

  template <bool IsRoot>
  ExiError handleElement(XMLNode* N) {
    DepthValidator X(BE);
    // IsRoot only used on root :)
    exi_try(handleSEAndAT<IsRoot>(N));
    if (auto* Child = N->first_node())
      exi_try(walkXMLContent(Child));
    return BE.EndElement();
  }

  ////////////////////////////////////////////////////////////////////////
  // StartTag/Element

  EXI_FLATTEN ExiError handleNode(XMLNode* N, const NodeKind K) {
    exi_invariant(CurrKinds.test(K));
    switch (K) {
    case NodeKind::node_element:
      return handleElement</*IsRoot=*/false>(N);
    case NodeKind::node_data:
      return BE.Characters(N->value());
    case NodeKind::node_cdata:
      return handleCDATA(*N);
    case NodeKind::node_comment:
      return handleCM(*N);
    case NodeKind::node_pi:
      return handlePI(*N);
    default:
      exi_guardrail("StartTagContent: Invalid NodeKind.");
    }
  }

  /// Depth-first traversal of the XML tree.
  /// Assumes the Node is not the root.
  EXI_NO_INLINE EXI_FLATTEN ExiError walkXMLContent(XMLNode* N) {
    exi_invariant(N /*&& IsSENode(N)*/ && !IsRootNode(N));
    do {
      const NodeKind K = N->type();
      if (!CurrKinds.test(K))
        continue;
      exi_try(handleNode(N, K));
    } while ((N = N->next_sibling()));
    return ExiError::OK;
  }

protected:
  EXI_NO_INLINE ExiError handleCDATA(XMLNode& N) const {
    SmallStr<80> Buf;
    raw_svector_ostream OS(Buf);

    if (Opts.PreserveCDATA)
      OS << "<![CDATA[" << N.value() << "]]>";
    else
      OS << escape::xml(N.value());

    return BE.Characters(Buf.str());
  }

  ExiError handleCM(XMLNode& N) const {
    return BE.Comment(N.value());
  }

  ExiError handlePI(XMLNode& N) const {
    if (N.name_size() == 0) {
      LOG_ERROR("Invalid processing instruction! Expected name.");
      return ErrorCode::kInvalidEXIInput;
    }
    return BE.ProcessingInstruction(N.name(), N.value());
  }

  ExiError handleDT(XMLNode& N) const {
    auto DTD = DTDParser::CreateDTEvent(N.value());
    if EXI_UNLIKELY(DTD.is_err())
      return DTD.error();
    return BE.Doctype(*DTD);
  }

  ExiError handleDocContent(XMLNode& N) const {
    const NodeKind K = N.type();
    if (!CurrKinds.test(K))
      return ExiError::OK;
    switch (K) {
    case NodeKind::node_comment:
      tail_return handleCM(N);
    case NodeKind::node_pi:
      tail_return handlePI(N);
    case NodeKind::node_doctype:
      tail_return handleDT(N);
    default:
      exi_guardrail("DocContent: impossible state.");
    }
  }

  ExiError handleDocEnd(XMLNode& N) const {
    const NodeKind K = N.type();
    if (!CurrKinds.test(K))
      return ExiError::OK;
    switch (K) {
    case NodeKind::node_comment:
      tail_return handleCM(N);
    case NodeKind::node_pi:
      tail_return handlePI(N);
    default:
      exi_guardrail("DocEnd: impossible state.");
    }
  }

  ExiError handleTopLevelSE(XMLNode* Root) {
    if EXI_UNLIKELY(!IsRootNode(Root)) {
      LOG_ERROR("non-root SE passed to handleTopLevelSE!");
      return ErrorCode::kInvalidEXIInput;
    }
    tail_return handleElement</*IsRoot=*/true>(Root);
  }
};

} // namespace INTERNAL_NS

static NodeKindBitset MakeNodeKindBitset(const BodyEncoder& BE) {
  using enum NodeKind;
  const auto Preserve = BE.getOptions().Preserve;
  NodeKindBitset B = kNKBitsetDefault;
  B.set(node_comment, Preserve.Comments);
  B.set(node_doctype, Preserve.DTDs);
  B.set(node_pi,      Preserve.PIs);
  return B; 
}

GenericXMLEncoderRunner::GenericXMLEncoderRunner(BodyEncoder& BE)
 : AllowedKinds(MakeNodeKindBitset(BE)) {
  // ...
}

//////////////////////////////////////////////////////////////////////////
// Interface

template <class Encoder>
static ExiError RunAs(XMLDocument& Doc, BodyEncoder* BE,
                      const XMLSerializerOpts& Opts) {
  auto& EE = cast<Encoder>(*BE);
  XMLEncoderRunner<Encoder> Runner(EE, Opts);
  return Runner.run(Doc);
}

static ExiError Run(XMLDocument& Doc, BodyEncoder* BE,
                    const XMLSerializerOpts& Opts) {
  if EXI_ALWAYS(isa<OrderedEncoder>(*BE))
    tail_return RunAs<OrderedEncoder>(Doc, BE, Opts);
  LOG_ERROR("Non-ordered encoders have not been implemented!");
  return ExiError::TODO;
}

//===----------------------------------------------------------------===//
// [Owning]XMLSerializer
//===----------------------------------------------------------------===//

ExiError XMLSerializer::exec(BodyEncoder* BE) {
  if EXI_NEVER(Doc == nullptr) {
    LOG_ERROR("Null XML document!");
    return ErrorCode::kNullptrRef;
  }
  return Run(*Doc, BE, *this);
}

ExiError OwningXMLSerializer::exec(BodyEncoder* BE) {
  exi_todo("add parsing interface");
  return Run(Doc, BE, *this);
}
