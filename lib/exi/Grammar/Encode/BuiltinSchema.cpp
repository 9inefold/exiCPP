//===- exi/Grammar/Encode/BuiltinSchema.cpp -------------------------===//
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
/// This file defines the base for the builtin schema.
///
//===----------------------------------------------------------------===//

#include <exi/Grammar/EncoderSchema.hpp>
#include <core/Common/DenseMap.hpp>
#include <core/Common/EnumArray.hpp>
#include <core/Common/MMatch.hpp>
#include <core/Common/SmallVec.hpp>
#include <core/Common/STLExtras.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/Logging.hpp>
#include <core/Support/TrailingArray.hpp>
#include <core/Support/WithColor.hpp>
#include <exi/Basic/D/InternalMacros.hpp>
#include <exi/Basic/ExiOptions.hpp>
#include <exi/Encode/D/EventMappings.mac>
#include <exi/Grammar/BIBuilder.hpp>
#include <exi/Grammar/BIEventMap.hpp>
#include <exi/Encode/Grammar.hpp>
#include <exi/Stream/OrderedWriter.hpp>
#include <fmt/ranges.h>
#include "SchemaGet.hpp"

using namespace exi;
using namespace exi::encode;

#define DEBUG_TYPE "BuiltinSchema"

/// Emits diagnostic for an error.
EXI_ERROR_CC static void Diagnose(const ExiError& E) {
  if (E != ExiError::OK)
    errs() << E << '\n';
}
/// Emits diagnostic for an error.
template <typename T>
EXI_ERROR_CC EXI_MINSIZE static void Diagnose(const ExiResult<T>& Result) {
  exi_invariant(Result.is_err());
  if (Result.error() != ExiError::OK)
    errs() << Result.error() << '\n';
}

#define VISIT_CASE(FROM, TO)                                                  \
case SimpleEventTerm::FROM:                                                   \
  Fn(static_cast<const TO&>(Event));                                          \
  return;
#define VISIT_ARR_CASE(FROM, TO)                                              \
case SimpleEventTerm::FROM:                                                   \
  Fn(static_cast<const TO*>(Arr), N);                                         \
  return;

template <typename F>
EXI_MINSIZE static void VisitEvent(const BaseEvent& Event,
                                   SimpleEventTerm K, F&& Fn) {
  switch (K) {
    EXI_ENCODE_EVENT_MAPPINGS(VISIT_CASE)
  default:
    LOG_WARN("Invalid EventTerm: {}!", get_event_name(K));
    return;
  }
}
template <typename F>
EXI_MINSIZE static void VisitEvent(const void* Arr, usize N,
                                   SimpleEventTerm K, F&& Fn) {
  switch (K) {
    EXI_ENCODE_EVENT_MAPPINGS(VISIT_ARR_CASE)
  default:
    LOG_WARN("Invalid EventTerm[]: {}!", get_event_name(K));
    return;
  }
}

//===----------------------------------------------------------------===//
// Built-in Grammar
//===----------------------------------------------------------------===//

/// The transitions for schemaless encodings are defined as the following.
/// If SC is not enabled, then the ChildContentItems for StartTagContent will
/// be (0.3) instead.
///
/// Document:
///   SD DocContent           0
/// 
/// DocContent:
///   SE (*) DocEnd           0
///   DT DocContent           1.0
///   CM DocContent           1.1.0
///   PI DocContent           1.1.1
/// 
/// DocEnd:
///   ED                      0
///   CM DocEnd               1.0
///   PI DocEnd               1.1
/// 
/// StartTagContent:
///   EE                      0.0
///   AT (*) StartTagContent  0.1
///   NS StartTagContent      0.2
///   SC Fragment             0.3
///   ChildContentItems      (0.4)  
/// 
/// ElementContent:
///   EE                      0
///   ChildContentItems      (1.0)  
/// 
/// ChildContentItems (n.m):
///   SE (*) ElementContent  n. m
///   CH ElementContent      n.(m+1)
///   ER ElementContent      n.(m+2)
///   CM ElementContent      n.(m+3).0
///   PI ElementContent      n.(m+3).1
///

namespace INTERNAL_NS(exi) {

//===----------------------------------------------------------------===//
// Ordered Encoding
//===----------------------------------------------------------------===//

#define ORDERED_ARGS OrderedEncoder* OE, const BaseEvent& Event, SimpleEventTerm K
#define ORDERED_BARGS OrderedEncoder* OE, const void* Arr, usize N, SimpleEventTerm K
#define ORDERED_NEXT OE, Event, K
#define ORDERED_BNEXT OE, Arr, N, K

template <is_ordwriter_stream StrmT>
class INTERNAL_LINKAGE OrderedBuiltinSchema final : public BuiltinSchema {
  using enum BIGrammarState;
  using BuiltinSchema::State;
  using Get = encode::Schema::Get<StrmT>;

  /// The grammar state.
  BIGrammarState Current = Document;
  /// Maps `SimpleEventTerm`s to event codes.
  BIEventMap TMap;
  /// The grammar stack.
  SmallVec<encode::BuiltinGrammar*, 0> GStack;
  /// Stores all the SE grammars.
  DenseMap<LocalNameInfo*, BuiltinGrammar*> Grammars;

  OrderedBuiltinSchema(OrderedEncoder& OE, const BIEventMap& TMap) : TMap(TMap) {
#if !defined(NDEBUG) || EXI_ENABLE_DUMP
  if (hasDbgLogLevel(INFO))
    TMap.dump(OE.getOptions());
#endif
  }

public:
  static Box<OrderedBuiltinSchema> New(OrderedEncoder* OE,
                                       const BIEventMap& BIEM) {
    auto* TheSchema = new OrderedBuiltinSchema(*OE, BIEM);
    return Box<OrderedBuiltinSchema>(TheSchema);
  }
  template <is_exi_allocator Alloc>
  [[nodiscard]] static OrderedBuiltinSchema*
   New(OrderedEncoder* OE, const BIEventMap& BIEM, Alloc& A) {
    auto* TheSchema = A.template Allocate<OrderedBuiltinSchema>();
    return new(TheSchema) OrderedBuiltinSchema(*OE, BIEM);
  }
  static Box<OrderedBuiltinSchema> New(BodyEncoder* BE,
                                       const ExiOptions& Opts) {
    if (auto* OE = dyn_cast_if_present<OrderedEncoder>(BE))
      return New(OE, BIEventMap::New(Opts));
    LOG_ERROR("Input was null or not an ordered encoder!");
    return nullptr;
  }

  ////////////////////////////////////////////////////////////////////////
  // Event Encoding

  ExiError encode(BodyEncoder* BE, const BaseEvent& Event,
                                   SimpleEventTerm K) override {
    exi_expensive_invariant(isa<OrderedEncoder>(BE));
    return this->setEventImpl(static_cast<OrderedEncoder*>(BE), Event, K);
  }

  ExiError batchEncode(BodyEncoder* BE, const void* Arr, usize N,
                                        SimpleEventTerm K) override {
    exi_expensive_invariant(isa<OrderedEncoder>(BE));
    return this->batchEventsImpl</*IsRoot=*/false>(
      static_cast<OrderedEncoder*>(BE), Arr, N, K);
  }

  ExiError batchEncodeRoot(BodyEncoder* BE, const void* Arr, usize N,
                                            SimpleEventTerm K) override {
    exi_expensive_invariant(isa<OrderedEncoder>(BE));
    exi_assert(Current == StartTagContent);
    return this->batchEventsImpl</*IsRoot=*/true>(
      static_cast<OrderedEncoder*>(BE), Arr, N, K);
  }

private:
  ALWAYS_INLINE CC void encodePrecomputedCode(OrderedEncoder* OE,
                                              FullEventCode EC) {
    if constexpr (std::same_as<StrmT, BitWriter>)
      LOG_EXTRA("Code[0:2]: @{}:0b{:0{}b}", EC.Bits, EC.Data, EC.Bits);
    else
      LOG_EXTRA("Code[0:2]: @{}:0x{:0{}x}", EC.Bits, EC.Data, (EC.Bits / 8));
    Get::Writer(OE).writeBits64(EC.Data, EC.Bits);
  }
  ALWAYS_INLINE CC void encodePrecomputedCode(OrderedEncoder* OE,
                                              SecondLevelEventCode EC) {
    if constexpr (std::same_as<StrmT, BitWriter>)
      LOG_EXTRA("Code[1:2]: @{}:0b{:0{}b}", EC.Bits, EC.Data, EC.Bits);
    else
      LOG_EXTRA("Code[1:2]: @{}:0x{:0{}x}", EC.Bits, EC.Data, (EC.Bits / 8));
    Get::Writer(OE).writeBits64(EC.Data, EC.Bits);
  }

  template <bool IsStart, SimpleEventTerm K>
  ALWAYS_INLINE CC void encodeSLCode(OrderedEncoder* OE) {
    if constexpr (IsStart)
      encodePrecomputedCode(OE, TMap.mapStartTagContent(K));
    else
      encodePrecomputedCode(OE, TMap.mapElementContent(K));
  }

  template <SimpleEventTerm Term>
  CC_FLATTEN ExiError encodeDocContent(ORDERED_ARGS) {
    encodePrecomputedCode(OE, TMap.mapDocContent<Term>());
    // TODO: Handle top-level SE
    if constexpr (Term != SimpleEventTerm::SE)
      return this->encodeEventSimple(
        OE, event_cast<Term>(Event, K));
    else
      // TODO: Make this its own thing...
      return this->handleSE<DocContent>(ORDERED_NEXT);
  }
  // TODO: Flatten?
  template <SimpleEventTerm Term>
  CC ExiError encodeDocEnd(ORDERED_ARGS) {
    encodePrecomputedCode(OE, TMap.mapDocEnd<Term>());
    return this->encodeEventSimple(
      OE, event_cast<Term>(Event, K));
  }

  /// ENTRY POINT - Dispatches common events.
  CC_INLINE ExiError setEventImpl(ORDERED_ARGS) {
    LOG_POSITION(OE);
    this->logCurrentGrammar();
    this->logEvent(K);
    switch (Current) {
    case StartTagContent:
      tail_return this->handleStartTag(ORDERED_NEXT);
    case ElementContent:
      tail_return this->handleElement(ORDERED_NEXT);
    case Fragment:
      exi_unimplemented("SC elements are currently unsupported");
    default:
      tail_return this->setDocTerm(ORDERED_NEXT);
    }
  }

  CC_INLINE GNU_ATTR(cold) ExiError setDocTerm(ORDERED_ARGS) {
    switch (Current) {
    case Document:
      // Very rarely set. It only happens once at the start.
      tail_return this->handleDocument(ORDERED_NEXT);
    case DocContent:
      tail_return this->handleDocContent(ORDERED_NEXT);
    case DocEnd:
      tail_return this->handleDocEnd(ORDERED_NEXT);
    default:
      exi_unreachable("invalid state?");
    }
  }

  /// ENTRY POINT - Dispatches common batch events.
  template <bool IsRoot = false>
  CC_INLINE ExiError batchEventsImpl(ORDERED_BARGS) {
    LOG_POSITION(OE);
    this->logCurrentGrammar();
    this->logEventEx(Arr, N, K);
    switch (Current) {
    case StartTagContent:
      tail_return this->batchStartTag<IsRoot>(ORDERED_BNEXT);
    case ElementContent:
      if constexpr (!IsRoot)
        tail_return this->batchElement(ORDERED_BNEXT);
    case Fragment:
      if constexpr (!IsRoot)
        exi_unimplemented("SC elements are currently unsupported");
    default:
      exi_guardrail("invalid batching state?");
    }
  }

  ////////////////////////////////////////////////////////////////////////
  // States

  ALWAYS_INLINE ExiError handleDocument(ORDERED_ARGS) {
    // Document is always empty, and therefore never reads.
    this->transition(DocContent);
    return ExiError::OK;
  }

  CC ExiError handleDocContent(ORDERED_ARGS) {
    using enum SimpleEventTerm;
    static constexpr State S = DocContent;
    switch (eventmap::DocContentIdx(K)) {
    case map_doccontent_v<SE>:
      // This should only be called once, at the start of processing.
      exi_assert(GStack.empty() && Grammars.empty());
      tail_return encodeDocContent<SE>(ORDERED_NEXT);
    case map_doccontent_v<CM>:
      tail_return encodeDocContent<CM>(ORDERED_NEXT);
    case map_doccontent_v<PI>:
      tail_return encodeDocContent<PI>(ORDERED_NEXT);
    case map_doccontent_v<DT>:
      tail_return encodeDocContent<DT>(ORDERED_NEXT);
    default:
      exi_guardrail("invalid DocContent");
    }
  }

  CC ExiError handleDocEnd(ORDERED_ARGS) {
    using enum SimpleEventTerm;
    static constexpr State S = DocEnd;
    switch (eventmap::DocEndIdx(K)) {
    case map_docend_v<ED>:
      tail_return encodeDocEnd<ED>(ORDERED_NEXT);
    case map_docend_v<CM>:
      tail_return encodeDocEnd<CM>(ORDERED_NEXT);
    case map_docend_v<PI>:
      tail_return encodeDocEnd<PI>(ORDERED_NEXT);
    default:
      exi_guardrail("invalid DocEnd");
    }
  }

  CC GNU_ATTR(hot) ExiError handleStartTag(ORDERED_ARGS) {
    //exi_todo("Implement StartTagContent");
    switch (K) {
    case SimpleEventTerm::EE:
      tail_return this->handleEE<true>(ORDERED_NEXT);
    case SimpleEventTerm::AT:
      tail_return this->handleAT(ORDERED_NEXT);
    case SimpleEventTerm::NS:
      tail_return this->handleNS(ORDERED_NEXT);
    case SimpleEventTerm::SC:
      tail_return this->handleSC(ORDERED_NEXT);
    default:
      tail_return this->handleChildContent<true>(ORDERED_NEXT);
    }
  }

  CC GNU_ATTR(hot) ExiError handleElement(ORDERED_ARGS) {
    // exi_todo("Implement ElementContent");
    switch (K) {
    case SimpleEventTerm::EE:
      tail_return this->handleEE<false>(ORDERED_NEXT);
    default:
      tail_return this->handleChildContent<false>(ORDERED_NEXT);
    }
  }

  /// Events shared between StartTag and Element.
  template <bool IsStart>
  CC ExiError handleSharedContent(ORDERED_ARGS) {
    switch (K) {
    case SimpleEventTerm::EE:
      tail_return this->handleEE(ORDERED_NEXT);
    /*
    case SEQName:
      // SE(qname) events are cached.
      tail_return this->handleSEQName(D);
    case CHExtern:
      tail_return this->handleCH<true>(D);
    */
    default:
      tail_return this->handleChildContent<IsStart>(ORDERED_NEXT);
    }
  }

  /// Events under the ChildContentItems macro.
  template <bool IsStart>
  CC_INLINE ExiError handleChildContent(ORDERED_ARGS) {
    static constexpr State S = IsStart ? StartTagContent : ElementContent;
    static constexpr const char* UnreachableMsg
      = IsStart ? "invalid StartTagContent" : "invalid ElementContent";
    switch (K) {
    case SimpleEventTerm::SE:
      tail_return this->handleSE<S>(ORDERED_NEXT);
    case SimpleEventTerm::CH:
      tail_return this->handleCH<IsStart>(ORDERED_NEXT);
    case SimpleEventTerm::CM:
    case SimpleEventTerm::PI:
    case SimpleEventTerm::ER:
      tail_return this->handleUncommon<IsStart>(ORDERED_NEXT);
    default:
      exi_unreachable(UnreachableMsg);
    }
  }

  template <bool IsRoot = false>
  CC ExiError batchStartTag(ORDERED_BARGS) {
    switch (K) {
    case SimpleEventTerm::NS:
      tail_return this->batchNS<IsRoot>(ORDERED_BNEXT);
    case SimpleEventTerm::AT:
      if constexpr (!IsRoot)
        tail_return this->batchAT(ORDERED_BNEXT);
    case SimpleEventTerm::CH:
      if constexpr (!IsRoot)
        tail_return this->batchCHStart(ORDERED_BNEXT);
    default:
      exi_guardrail("invalid batch type.");
    }
  }

  CC ExiError batchElement(ORDERED_BARGS) {
    if EXI_ALWAYS(K == SimpleEventTerm::CH)
      tail_return this->batchCHElem(ORDERED_BNEXT);
    exi_guardrail("invalid batch type.");
  }

  ////////////////////////////////////////////////////////////////////////
  // Event Handling

  template <bool IsStart>
  ALWAYS_INLINE CC void handlePrevSEGrammar(OrderedEncoder* OE, LocalNameInfo* LN) {
    exi_invariant(!GStack.empty());
    auto* G = GStack.back();
    if (void* IP = G->setSETerm<IsStart>(&Get::Writer(OE), LN)) {
      this->encodeSLCode<IsStart, SimpleEventTerm::SE>(OE);
      G->addSETerm<IsStart>(OE, LN, IP);
    }
  }
  template <State S>
  void handleSEDefaultCode(OrderedEncoder* OE) {
    static constexpr bool IsStart = (S == StartTagContent);
    static_assert(S != DocContent);
    auto* G = GStack.back();
    G->writeFallbackCode<IsStart>(&Get::Writer(OE));
    this->encodeSLCode<IsStart, SimpleEventTerm::SE>(OE);
  }
  CC ExiError loadSEGrammar(OrderedEncoder* OE, LocalNameInfo* LN) {
    this->transition(StartTagContent);
    auto [G, Cached] = loadGrammar(OE, LN);
    GStack.push_back(G);
    return ExiError::OK;
  }

  template <State S>
  EXI_FLATTEN CC ExiError handleSE(ORDERED_ARGS) {
    return this->handleSE<S>(OE,
      event_cast<SimpleEventTerm::SE>(Event, K));
  }
  template <State S>
  CC ExiError handleSE(OrderedEncoder* OE, const StartElemEvent& SE) {
    if EXI_UNLIKELY(SE.Tag != 0)
      tail_return this->handleSEUri<S>(OE, SE);
    if constexpr (S != DocContent) {
      auto [URIV, LN] = OE->lookupSE(SE);
      if (LN != nullptr)
        return this->handlePrevSE<S>(OE, SE, LN);
      this->handleSEDefaultCode<S>(OE);
    }
    LocalNameInfo* LN = EXI_UNWRAP(OE->encodeSE<StrmT>(SE));
    return this->loadSEGrammar(OE, LN);
  }
  template <State S>
  CC ExiError handleSEUri(OrderedEncoder* OE, const StartElemEvent& SE) {
    auto& SEUri = static_cast<const StartElemURIEvent&>(SE);
    if constexpr (S != DocContent) {
      auto [URIV, LN] = OE->lookupSEUri(SEUri);
      if (LN != nullptr)
        return this->handlePrevSEUri<S>(OE, SEUri, LN);
      this->handleSEDefaultCode<S>(OE);
    }
    LocalNameInfo* LN = EXI_UNWRAP(OE->encodeSEUri<StrmT>(SEUri));
    return this->loadSEGrammar(OE, LN);
  }
  template <State S>
  CC ExiError handlePrevSE(OrderedEncoder* OE,
                           const StartElemEvent& SE, LocalNameInfo* LN) {
    static constexpr bool IsStart = (S == StartTagContent);
    this->handlePrevSEGrammar<IsStart>(OE, LN);
    // TODO: Encode previous!
    auto* LN2 = EXI_UNWRAP(OE->encodeSE<StrmT>(SE));
    exi_invariant(LN2 == LN);
    return this->loadSEGrammar(OE, LN);
  }
  template <State S>
  CC ExiError handlePrevSEUri(OrderedEncoder* OE,
                              const StartElemURIEvent& SE, LocalNameInfo* LN) {
    static constexpr bool IsStart = (S == StartTagContent);
    this->handlePrevSEGrammar<IsStart>(OE, LN);
    // TODO: Encode previous!
    auto* LN2 = EXI_UNWRAP(OE->encodeSEUri<StrmT>(SE));
    exi_invariant(LN2 == LN);
    return this->loadSEGrammar(OE, LN);
  }

  /// Since element grammars always have single term EE event codes, we don't
  /// need to add it to the grammar.
  template <bool IsStart = false>
  CC ExiError handleEE(ORDERED_ARGS) {
    exi_invariant(!GStack.empty());
    auto* G = GStack.back();
    if (G->setEETerm<IsStart>(&Get::Writer(OE))) {
      exi_invariant(IsStart, "Invalid EE!");
      // Must be StartTagContent.
      this->encodePrecomputedCode(OE,
        TMap.mapStartTagContent(SimpleEventTerm::EE));
      auto* EENode = new (*OE) gnode::EENode;
      G->addEETerm<IsStart>(EENode);
    }
    GStack.pop_back();
    if EXI_LIKELY(!GStack.empty())
      this->transition(ElementContent);
    else
      this->transition(DocEnd);
    return ExiError::OK;
  }

  EXI_FLATTEN CC ExiError handleAT(ORDERED_ARGS) {
    return this->handleAT(OE,
      event_cast<SimpleEventTerm::AT>(Event, K));
  }
  CC ExiError handleAT(OrderedEncoder* OE, const AttrEvent& AT) {
    exi_invariant(!GStack.empty() && GStack.back());
    auto* G = GStack.back();
    auto [URIV, LN] = OE->lookupAT(AT);
    if (LN == nullptr) {
      G->writeFallbackCode<true>(&Get::Writer(OE));
      this->encodeSLCode<true, SimpleEventTerm::AT>(OE);
      LN = EXI_UNWRAP(OE->encodeAT<StrmT>(AT));
      G->addNewATTerm(OE, LN);
      return ExiError::OK;
    }
    // LocalName does exist.
    if (void* IP = G->setATTerm(&Get::Writer(OE), LN)) {
      this->encodeSLCode<true, SimpleEventTerm::AT>(OE);
      G->addATTerm(OE, LN, IP);
    }
    return OE->encodeATKnown<StrmT>(AT);
  }

  template <bool IsRoot = false>
  EXI_FLATTEN CC ExiError handleNS(ORDERED_ARGS) {
    return this->handleNS<IsRoot>(OE,
      event_cast<SimpleEventTerm::NS>(Event, K));
  }
  template <bool IsRoot = false>
  CC ExiError handleNS(OrderedEncoder* OE, const NamespaceEvent& NS) {
    if (OE->PreservePrefixes()) {
      StrmT* OW = &Get::Writer(OE);
      GStack.back()->writeFallbackCode<true>(OW);
      this->encodeSLCode<true, SimpleEventTerm::NS>(OE);
      return OE->encodeNS<StrmT, IsRoot>(NS);
    }
    return OE->saveNSToTableOnly<IsRoot>(NS);
  }

  template <bool IsStart>
  EXI_FLATTEN CC ExiError handleCH(ORDERED_ARGS) {
    return this->handleCH<IsStart>(OE,
      event_cast<SimpleEventTerm::CH>(Event, K));
  }
  template <bool IsStart>
  CC ExiError handleCH(OrderedEncoder* OE, const CharEvent& CH) {
    exi_invariant(!GStack.empty());
    auto* G = GStack.back();
    if (G->setCHTerm<IsStart>(&Get::Writer(OE))) {
      this->encodeSLCode<IsStart, SimpleEventTerm::CH>(OE);
      auto* CHNode = new (*OE) gnode::CHNode;
      G->addCHTerm<IsStart>(CHNode);
    }
    if constexpr (IsStart)
      this->transition(ElementContent);
    return OE->encodeValue<StrmT>(G->getName(), CH.name());
  }

  EXI_NO_INLINE CC ExiError handleSC(ORDERED_ARGS) {
    LOG_ERROR("SC events are not supported.");
    this->transition(Fragment);
    return ExiError::TODO;
  }

  template <bool IsStart>
  CC ExiError handleUncommon(ORDERED_ARGS) {
    StrmT* OW = &Get::Writer(OE);
    GStack.back()->writeFallbackCode<IsStart>(OW);
    if constexpr (IsStart) {
      encodePrecomputedCode(OE,
        TMap.mapStartTagContent(K));
      this->transition(ElementContent);
    } else {
      encodePrecomputedCode(OE,
        TMap.mapElementContent(K));
    }
    tail_return this->encodeUncommon(ORDERED_NEXT);
  }

  CC ExiError encodeUncommon(ORDERED_ARGS) {
    using enum SimpleEventTerm;
    switch (K) {
    case SimpleEventTerm::CM:
      return this->encodeEventSimple(
        OE, event_cast<SimpleEventTerm::CM>(Event));
    case SimpleEventTerm::PI:
      return this->encodeEventSimple(
        OE, event_cast<SimpleEventTerm::PI>(Event));
    case SimpleEventTerm::DT:
      return this->encodeEventSimple(
        OE, event_cast<SimpleEventTerm::DT>(Event));
    default:
      exi_guardrail("uncommon type not in {CH,PI,ER}");
    }
  }

  // Batching
  // TODO: Add optimizations specific to batches.

  CC ExiError batchAT(ORDERED_BARGS) {
    auto* VArr = static_cast<const AttrEvent*>(Arr);
    for (usize Ix = 0; Ix < N - 1; ++Ix) {
      this->logEvent(K);
      exi_try(handleAT(OE, VArr[Ix]));
    }
    this->logEvent(K);
    return handleAT(OE, VArr[N - 1]);
  }

  template <bool IsRoot = false>
  CC ExiError batchNS(ORDERED_BARGS) {
    auto* VArr = static_cast<const NamespaceEvent*>(Arr);
    for (usize Ix = 0; Ix < N - 1; ++Ix) {
      this->logEvent(K);
      exi_try(handleNS<IsRoot>(OE, VArr[Ix]));
    }
    this->logEvent(K);
    return handleNS<IsRoot>(OE, VArr[N - 1]);
  }

  CC ExiError batchCHElem(ORDERED_BARGS) {
    auto* VArr = static_cast<const CharEvent*>(Arr);
    for (usize Ix = 0; Ix < N - 1; ++Ix) {
      this->logEvent(K);
      exi_try(handleCH<false>(OE, VArr[Ix]));
    }
    this->logEvent(K);
    return handleCH<false>(OE, VArr[N - 1]);
  }

  CC ExiError batchCHStart(ORDERED_BARGS) {
    auto* VArr = static_cast<const CharEvent*>(Arr);
    this->logEvent(K);
    if (N <= 1)
      return handleCH<true>(OE, *VArr);
    exi_try(handleCH<true>(OE, *VArr));
    tail_return this->batchCHElem(OE, VArr + 1, N - 1, K);
  }

  ////////////////////////////////////////////////////////////////////////
  // Value Encoding

  /// Encodes a simple event - can be {SD, ED}.
  CC ExiError encodeEventSimple(OrderedEncoder*, const NoEventData&) {
    return ExiError::OK;
  }
  /// Encodes a simple event - can be {CM, ER}.
  CC ExiError encodeEventSimple(OrderedEncoder* OE,
                                const StringEventData& Event) {
    Get::Writer(OE).encodeString(
      StrRef(Event.Data, Event.Size));
    return ExiError::OK;
  }
  /// Encodes a simple event - can be {PI}.
  CC ExiError encodeEventSimple(OrderedEncoder* OE,
                                const ProcInstrEvent& Event) {
    StrmT& Strm = Get::Writer(OE);
    Strm.encodeString(Event[0]);
    Strm.encodeString(Event[1]);
    return ExiError::OK;
  }
  /// Encodes a simple event - can be {DT}.
  inline CC ExiError encodeEventSimple(OrderedEncoder* OE,
                                       const DoctypeEvent& Event);

  ////////////////////////////////////////////////////////////////////////
  // Grammar

  ALWAYS_INLINE CC void transition(State New) { Current = New; }

  /// Returns `[Grammar, Cached]`.
  std::pair<BuiltinGrammar*, bool>
   loadGrammar(OrderedEncoder* OE, LocalNameInfo* LN) {
    if (auto* G = Grammars.lookup(LN))
      return {G, true};
    // Cache miss
    auto* G = this->makeGrammar(OE, LN);
    return {G, false};
  }

  BuiltinGrammar* makeGrammar(OrderedEncoder* OE, LocalNameInfo* LN) {
    auto G = new (*OE) BuiltinGrammar(LN);
    auto [It, DidEmplace] = Grammars.try_emplace(LN, G);
    exi_invariant(DidEmplace, "grammar already added");
    return G;
  }

  ////////////////////////////////////////////////////////////////////////
  // Printing

public:
  void dump() const override {}
  // ...

private:
#if EXI_LOGGING
  EXI_PRESERVE_CALLSITE void logCurrentGrammar();
  EXI_PRESERVE_CALLSITE void logEvent(SimpleEventTerm Term);
  EXI_PRESERVE_CALLSITE void logEventEx(const BaseEvent& Event, SimpleEventTerm K);
  EXI_PRESERVE_CALLSITE void logEventEx(const void*, usize N, SimpleEventTerm K);
#else
  ALWAYS_INLINE constexpr void logCurrentGrammar() {}
  ALWAYS_INLINE constexpr void logEvent(SimpleEventTerm) {}
  ALWAYS_INLINE constexpr void logEventEx(const BaseEvent&, SimpleEventTerm) {}
  ALWAYS_INLINE constexpr void logEventEx(const void*, usize, SimpleEventTerm) {}
#endif
  void anchor() override;
};

template <is_ordwriter_stream StrmT>
CC ExiError OrderedBuiltinSchema<StrmT>::encodeEventSimple(
 OrderedEncoder* OE, const DoctypeEvent& Event) {
  StrmT& Strm = Get::Writer(OE);
  switch (Event.Kind) {
  case DTK_None:
  case DTK_Text:
    Strm.encodeString(Event[0]); // Name
    Strm.writeBits(ubit<16>(0)); // Padding[2]
    Strm.encodeString(Event[1]); // Text?
    break;
  case DTK_System:
    Strm.encodeString(Event[0]); // Name
    Strm.writeByte(0);           // Padding[1]
    Strm.encodeString(Event[1]); // "sysid"
    Strm.encodeString(Event[2]); // Text?
    break;
  case DTK_Public:
    Strm.encodeString(Event[0]); // Name
    Strm.encodeString(Event[1]); // "pubid"
    Strm.encodeString(Event[2]); // "sysid"
    Strm.encodeString(Event[3]); // Text?
    break;
  }
  return ExiError::OK;
}

//===----------------------------------------------------------------===//
// Channel Encoding (soon)
//===----------------------------------------------------------------===//

#define CHANNEL_ARGS ChannelEncoder* CE, const BaseEvent& Event, SimpleEventTerm K
#define CHANNEL_NEXT CE, Event, K

// ...

} // namespace INTERNAL_NS

template<> void OrderedBuiltinSchema<BitWriter>::anchor() {}
template<> void OrderedBuiltinSchema<ByteWriter>::anchor() {}

//===----------------------------------------------------------------===//
// Logging
//===----------------------------------------------------------------===//

#if EXI_LOGGING
template <is_ordwriter_stream StrmT>
EXI_PRESERVE_CALLSITE void OrderedBuiltinSchema<StrmT>::logCurrentGrammar() {
  using enum raw_ostream::Colors;
  if (!hasDbgLogLevel(VERBOSE))
    // Don't do any work if log level is insufficient.
    return;
  
  WithColor OS(dbgs(), BRIGHT_WHITE);
  OS << "\nState: " << get_state_name(Current);
  const bool IsContent = mmatch(Current).is(StartTagContent, ElementContent);
  if EXI_LIKELY(IsContent && !GStack.empty()) {
    LocalNameInfo* LN = GStack.back()->getName();
    StrRef URI = LN->uriName(), Name = LN->name();
    if (URI.empty())
      OS << format("[{}]", Name);
    else
      OS << format("[{}:{}]", URI, Name);
  }
  OS << '\n';
}

template <is_ordwriter_stream StrmT>
EXI_PRESERVE_CALLSITE void OrderedBuiltinSchema<StrmT>::logEvent(SimpleEventTerm Term) {
  LOG_INFO("> With {}: {}",
    get_event_name(Term),
    get_event_signature(Term)
  );
}

template <is_ordwriter_stream StrmT>
EXI_PRESERVE_CALLSITE void OrderedBuiltinSchema<StrmT>::logEventEx(
 const BaseEvent& Event, SimpleEventTerm K) {
  if (!hasDbgLogLevel(VERBOSE))
    return;
  VisitEvent(Event, K, [] <typename T> (T& Event) EXI_MINSIZE {
    SmallStr<64> Str;
    raw_svector_ostream OS(Str);
    OS << "> With " << get_event_name(Event) << ": ";
    if constexpr (is_empty_event<T>)
      OS << get_event_signature(unmap_event_v<T>);
    else
      OS << Event;
    LOG_EXTRA("{}", Str.str());
  });
}
template <is_ordwriter_stream StrmT>
EXI_PRESERVE_CALLSITE void OrderedBuiltinSchema<StrmT>::logEventEx(
 const void*, usize N, SimpleEventTerm K) {
  if (!hasDbgLogLevel(VERBOSE))
    return;
  WithColor(dbgs(), raw_ostream::BRIGHT_MAGENTA)
    << format("Batch of {}[{}]\n", get_event_name(K), N);
}
#endif // EXI_LOGGING

//===----------------------------------------------------------------===//
// Getters
//===----------------------------------------------------------------===//

namespace INTERNAL_NS(exi) {

template <is_ordwriter_stream StrmT>
class OrderedBISchemaFactory {
  BIEventMap BIEM;
public:
  OrderedBISchemaFactory(const ExiOptions& Opts)
   : BIEM(BIEventMap::New(Opts)) {}
  
  Box<BuiltinSchema> operator()(BodyEncoder* BE) const {
    if (auto* OE = dyn_cast<OrderedEncoder>(BE)) [[likely]]
      return OrderedBuiltinSchema<StrmT>::New(OE, BIEM);
    LOG_ERROR("Incorrect BodyEncoder type passed to SchemaFactory!");
    return nullptr;
  }
};

} // namespace INTERNAL_NS

static factory_t NewChanneled(const ExiOptions& Opts) {
  exi_todo("channel readers are currently unsupported!");
}

factory_t BuiltinSchema::New(const ExiOptions& Opts) {
  switch (Opts.Alignment) {
  case AlignKind::BitPacked:
    return OrderedBISchemaFactory<BitWriter>(Opts);
  case AlignKind::BytePacked:
    return OrderedBISchemaFactory<ByteWriter>(Opts);
  case AlignKind::PreCompression:
    return NewChanneled(Opts);
  case AlignKind::None:
    LOG_ERROR("AlignKind cannot be None!");
    return nullptr;
  }
  exi_unreachable("invalid alignment!");
}

static Box<BuiltinSchema> MakeChanneled(const ExiOptions& Opts) {
  exi_todo("channel readers are currently unsupported!");
}

Box<BuiltinSchema> BuiltinSchema::Make(const ExiOptions& Opts, BodyEncoder* BE) {
  switch (Opts.Alignment) {
  case AlignKind::BitPacked:
    return OrderedBuiltinSchema<BitWriter>::New(BE, Opts);
  case AlignKind::BytePacked:
    return OrderedBuiltinSchema<ByteWriter>::New(BE, Opts);
  case AlignKind::PreCompression:
    return MakeChanneled(Opts);
  case AlignKind::None:
    LOG_ERROR("AlignKind cannot be None!");
    return nullptr;
  }
  exi_unreachable("invalid alignment!");
}
