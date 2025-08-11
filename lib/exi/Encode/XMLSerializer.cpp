//===- exi/Encode/XMLSerializer.cpp ----------------------------------===//
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
/// This file implements the interface used to encode XML as EXI.
///
//===----------------------------------------------------------------===//

#include <exi/Encode/XMLSerializer.hpp>
#include <core/Common/EnumArray.hpp>
#include <core/Common/StringSwitch.hpp>
#include <core/Support/Alignment.hpp>
#include <core/Support/Casting.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/D/InternalMacros.hpp>
#include <exi/Encode/Event.hpp>
//#include <Encode/ChannelEncoder-inl.hpp>
#include <Encode/OrderedEncoder-inl.hpp>
#include <bitset>

CLANG_IGNORED("-Wunused-label")

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

public:
  XMLEncoderRunner(Encoder& BE)
   : GenericXMLEncoderRunner(BE),
      BE(BE), TheSchema(BE.getSchema()),
      CurrKinds(0b1) {
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
    exi_todo("StartTagContent/ElementContent");

  // DocEnd:
    this->andKinds(kNKBitsetDocEnd);
    N = N->next_sibling();
    if (needsDocEndLoop()) while(N) {
      exi_try(handleDocEnd(*N));
      N = N->next_sibling();
    }
    return BE.EndDocument();
  }

protected:
  static StrRef TakeToken(StrRef& S) {
    const auto Pos = S.find_first_of(' ');
    if (Pos == StrRef::npos) {
      StrRef Out = S;
      S = "";
      return Out;
    }
    StrRef Out = S.take_front(Pos);
    S = S.drop_front(Pos).drop_while([](char C) {
      return C == ' ';
    });
    return Out;
  }

  static ExiError StripDTText(StrRef& S) {
    if EXI_LIKELY(S.consume_pinch("[", "]")) {
      S = S.trim(' ');
      return ExiError::OK;
    }
    LOG_ERROR("Invalid DOCTYPE! Expected [<text>], got '{}'", S);
    return ErrorCode::kInvalidEXIInput;
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
    StrRef Data = N.value();
    StrRef Name = TakeToken(Data);
    // Name only
    if (Data.empty()) {
      if EXI_UNLIKELY(Name.empty()) {
        LOG_ERROR("Invalid DOCTYPE! Expected Name.");
        return ErrorCode::kInvalidEXIInput;
      }
      return BE.Doctype(make_event<DT>(DTK_None, Name));
    } else if (Data.starts_with('['))
      return BE.Doctype(make_event<DT>(DTK_None, Name, Data));

    StrRef Kind = TakeToken(Data);
    auto K = StringSwitch<DoctypeKind>(Kind)
      .Case("SYSTEM", DTK_System)
      .Case("PUBLIC", DTK_Public)
      .Default(DTK_None);
    if (K == DTK_None) {
      LOG_ERROR("Invalid DOCTYPE! "
                "Expected SYSTEM or PUBLIC, got '{}'.", Kind);
      return ErrorCode::kInvalidEXIInput;
    }
    // SYSTEM
    StrRef PrimID = TakeToken(Data);
    if (K == DTK_System) {
      if (!Data.empty())
        exi_try(StripDTText(Data));
      return BE.Doctype(make_event<DT>(
        DTK_System, Name, PrimID, Data));
    }
    // PUBLIC
    StrRef SysID = TakeToken(Data);
    if (!Data.empty())
      exi_try(StripDTText(Data));
    return BE.Doctype(make_event<DT>(
      DTK_Public, Name, PrimID, SysID, Data));
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
      exi_unreachable("DocContent: impossible state.");
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
      exi_unreachable("DocEnd: impossible state.");
    }
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
static ExiError RunAs(XMLDocument& Doc, BodyEncoder* BE) {
  auto& EE = cast<Encoder>(*BE);
  XMLEncoderRunner<Encoder> Runner(EE);
  return Runner.run(Doc);
}

static ExiError Run(XMLDocument& Doc, BodyEncoder* BE) {
  if EXI_NEVER(!isa<OrderedEncoder>(*BE)) {
    LOG_ERROR("Non-ordered encoders have not been implemented!");
    return ExiError::TODO;
  }
  tail_return RunAs<OrderedEncoder>(Doc, BE);
}

//===----------------------------------------------------------------===//
// [Owning]XMLSerializer
//===----------------------------------------------------------------===//

ExiError XMLSerializer::run(BodyEncoder* BE) {
  if EXI_NEVER(Doc == nullptr) {
    LOG_ERROR("Null XML document!");
    return ErrorCode::kNullptrRef;
  }
  return Run(*Doc, BE);
}

ExiError OwningXMLSerializer::run(BodyEncoder* BE) {
  exi_todo("add parsing interface");
  return Run(Doc, BE);
}
