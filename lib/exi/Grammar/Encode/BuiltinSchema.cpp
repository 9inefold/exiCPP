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
#define ORDERED_NEXT OE, Event, K
#define ORDERED_BARGS OrderedEncoder* OE, const void* Arr, usize N, SimpleEventTerm K
#define ORDERED_BNEXT OE, Arr, N, K

template <is_ordwriter_stream StrmT>
class INTERNAL_LINKAGE OrderedBuiltinSchema final : public BuiltinSchema {
  using enum BIGrammarState;
  using BuiltinSchema::State;

  using Get = encode::Schema::Get<StrmT>;
  using GrammarT = PointerIntPair<BuiltinGrammar*, 1, bool>;

  /// Contains info on the compressed grammars.
  BIInfoArray Info;
  /// The pseudo grammar stack.
  BIGrammarState Current = Document;
  /// ...
  /// Maps `SimpleEventTerm`s to event codes.
  BIEventMap TMap;

  OrderedBuiltinSchema(OrderedEncoder&, const BIEventMap& TMap) : TMap(TMap) {}

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
      LOG_EXTRA(">> Code: 0b{:0{}b}", EC.Data, EC.Bits);
    else
      LOG_EXTRA(">> Code: 0x{:0{}x}", EC.Data, (EC.Bits / 8));
    Get::Writer(OE).writeBits64(EC.Data, EC.Bits);
  }
  ALWAYS_INLINE CC void encodePrecomputedCode(OrderedEncoder* OE,
                                              SecondLevelEventCode EC) {
    if constexpr (std::same_as<StrmT, BitWriter>)
      LOG_EXTRA(">> S-Code: 0b{:0{}b}", EC.Data, EC.Bits);
    else
      LOG_EXTRA(">> S-Code: 0x{:0{}x}", EC.Data, (EC.Bits / 8));
    Get::Writer(OE).writeBits64(EC.Data, EC.Bits);
  }

  template <SimpleEventTerm Term>
  CC_FLATTEN ExiError encodeDocContent(ORDERED_ARGS) {
    encodePrecomputedCode(OE, TMap.mapDocContent<Term>());
    // TODO: Handle top-level SE
    if constexpr (Term != SimpleEventTerm::SE)
      return this->encodeEventS(
        OE, event_cast<Term>(Event, K));
    else
      return this->handleSE</*IsRoot=*/true>(ORDERED_NEXT);
  }
  // TODO: Flatten?
  template <SimpleEventTerm Term>
  CC ExiError encodeDocEnd(ORDERED_ARGS) {
    encodePrecomputedCode(OE, TMap.mapDocEnd<Term>());
    return this->encodeEventS(
      OE, event_cast<Term>(Event, K));
  }

  /// ENTRY POINT - Dispatches common events.
  CC_INLINE ExiError setEventImpl(ORDERED_ARGS) {
    this->logEvent(Event, K);
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
    this->logEvent(Arr, N, K);
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
    this->pushGrammar(DocContent);
    return ExiError::OK;
  }

  CC ExiError handleDocContent(ORDERED_ARGS) {
    using enum SimpleEventTerm;
    static constexpr State S = DocContent;
    switch (BIEventMap::IdxDocContent(K)) {
    case map_doccontent_v<SE>:
      // This should only be called once, at the start of processing.
      //exi_assert(GStack.empty() && Grammars.empty());
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
    switch (BIEventMap::IdxDocEnd(K)) {
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
    exi_todo("Implement StartTagContent");
  }

  CC GNU_ATTR(hot) ExiError handleElement(ORDERED_ARGS) {
    exi_todo("Implement ElementContent");
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
    static constexpr const char* UnreachableMsg
      = IsStart ? "invalid StartTagContent" : "invalid ElementContent";
    switch (K) {
    case SimpleEventTerm::SE:
      tail_return this->handleSE(ORDERED_NEXT);
    case SimpleEventTerm::CH:
      tail_return this->handleCH(ORDERED_NEXT);
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
        tail_return this->batchCH(ORDERED_BNEXT);
    default:
      exi_guardrail("invalid batch type.");
    }
  }

  CC ExiError batchElement(ORDERED_BARGS) {
    exi_todo("Implement ElementContent batching");
    switch (K) {
    case SimpleEventTerm::NS:
      tail_return this->batchNS(ORDERED_BNEXT);
    case SimpleEventTerm::AT:
      tail_return this->batchAT(ORDERED_BNEXT);
    case SimpleEventTerm::CH:
      tail_return this->batchCH(ORDERED_BNEXT);
    default:
      exi_guardrail("invalid batch type.");
    }
  }

  ////////////////////////////////////////////////////////////////////////
  // Event Handling

  template <bool IsRoot = false>
  EXI_FLATTEN CC ExiError handleSE(ORDERED_ARGS) {
    return this->handleSE<IsRoot>(OE,
      event_cast<SimpleEventTerm::SE>(Event, K));
  }
  template <bool IsRoot = false>
  CC ExiError handleSE(OrderedEncoder* OE, const StartElemEvent& SE) {
    exi_todo("Implement SE");
  }

  /// Since element grammars always have single term EE event codes, we don't
  /// need to add it to the grammar.
  template <bool IsStart = false>
  CC ExiError handleEE(ORDERED_ARGS) {
    exi_todo("Implement EE");
  }

  EXI_FLATTEN CC ExiError handleAT(ORDERED_ARGS) {
    return this->handleAT(OE,
      event_cast<SimpleEventTerm::AT>(Event, K));
  }
  CC ExiError handleAT(OrderedEncoder* OE, const AttrEvent& AT) {
    exi_todo("Implement AT");
  }

  template <bool IsRoot = false>
  EXI_FLATTEN CC ExiError handleNS(ORDERED_ARGS) {
    return this->handleSE<IsRoot>(OE,
      event_cast<SimpleEventTerm::NS>(Event, K));
  }
  template <bool IsRoot = false>
  CC ExiError handleNS(OrderedEncoder* OE, const NamespaceEvent& NS) {
    exi_todo("Implement NS");
  }

  EXI_FLATTEN CC ExiError handleCH(ORDERED_ARGS) {
    return this->handleCH(OE,
      event_cast<SimpleEventTerm::CH>(Event, K));
  }
  CC ExiError handleCH(OrderedEncoder* OE, const CharEvent& NS) {
    exi_todo("Implement CH");
  }

  template <bool IsStart>
  CC ExiError handleUncommon(ORDERED_ARGS) {
    exi_todo("Add first-level event code");
    // Place first-level event code.
    if constexpr (IsStart) {
      TMap.mapStartTagContent(K);
      this->pushGrammar(ElementContent);
    } else
      TMap.mapElementContent(K);
    exi_todo("Implement {CH,PI,ER}");
  }

  // Batching
  // TODO: Add optimizations specific to batches.

  CC ExiError batchAT(ORDERED_BARGS) {
    auto* VArr = static_cast<const AttrEvent*>(Arr);
    for (usize Ix = 0; Ix < N - 1; ++Ix)
      exi_try(handleAT(OE, VArr[Ix]));
    return handleAT(OE, VArr[N - 1]);
  }

  template <bool IsRoot = false>
  CC ExiError batchNS(ORDERED_BARGS) {
    auto* VArr = static_cast<const NamespaceEvent*>(Arr);
    for (usize Ix = 0; Ix < N - 1; ++Ix)
      exi_try(handleNS<IsRoot>(OE, VArr[Ix]));
    return handleNS<IsRoot>(OE, VArr[N - 1]);
  }

  CC ExiError batchCH(ORDERED_BARGS) {
    auto* VArr = static_cast<const CharEvent*>(Arr);
    for (usize Ix = 0; Ix < N - 1; ++Ix)
      exi_try(handleCH(OE, VArr[Ix]));
    return handleCH(OE, VArr[N - 1]);
  }

  ////////////////////////////////////////////////////////////////////////
  // Value Encoding

  /// Encodes a simple event - can be {SD, ED}.
  CC ExiError encodeEventS(OrderedEncoder*, const NoEventData&) {
    return ExiError::OK;
  }
  /// Encodes a simple event - can be {CM, ER}.
  CC ExiError encodeEventS(OrderedEncoder* OE,
                           const StringEventData& Event) {
    Get::Writer(OE).encodeString(
      StrRef(Event.Data, Event.Size));
    return ExiError::OK;
  }
  /// Encodes a simple event - can be {PI}.
  CC ExiError encodeEventS(OrderedEncoder* OE,
                           const ProcInstrEvent& Event) {
    StrmT& Strm = Get::Writer(OE);
    Strm.encodeString(Event[0]);
    Strm.encodeString(Event[1]);
    return ExiError::OK;
  }
  /// Encodes a simple event - can be {DT}.
  inline CC ExiError encodeEventS(OrderedEncoder* OE,
                                  const DoctypeEvent& Event);

  ////////////////////////////////////////////////////////////////////////
  // Grammar

  ALWAYS_INLINE CC void pushGrammar(State New) { Current = New; }

  ////////////////////////////////////////////////////////////////////////
  // Printing

public:
  void dump() const override {}
  // ...

private:
#if EXI_LOGGING
  //EXI_PRESERVE_CALLSITE void logCurrentGrammar(ExiDecoder* D);
  //EXI_PRESERVE_CALLSITE void logCurrentEvent();
  EXI_PRESERVE_CALLSITE void logEvent(const BaseEvent& Event, SimpleEventTerm K);
  EXI_PRESERVE_CALLSITE void logEvent(const void* Arr, usize N, SimpleEventTerm K);
#else
  //ALWAYS_INLINE constexpr void logCurrentGrammar(ExiDecoder*) {}
  //ALWAYS_INLINE constexpr void logCurrentEvent() {}
  ALWAYS_INLINE constexpr void logEvent(const BaseEvent&, SimpleEventTerm) {}
  ALWAYS_INLINE constexpr void logEvent(const void*, usize, SimpleEventTerm) {}
#endif
  void anchor() override;
};

template <is_ordwriter_stream StrmT>
CC ExiError OrderedBuiltinSchema<StrmT>::encodeEventS(
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
EXI_PRESERVE_CALLSITE void OrderedBuiltinSchema<StrmT>::logEvent(
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
EXI_PRESERVE_CALLSITE void OrderedBuiltinSchema<StrmT>::logEvent(
 const void* Arr, usize N, SimpleEventTerm K) {
  if (!hasDbgLogLevel(VERBOSE))
    return;
  VisitEvent(Arr, N, K, [] <typename T> (T*, usize N) EXI_MINSIZE {
    LOG_EXTRA("> Batch of {}[{}]",
      get_event_name(unmap_event_v<T>), N);
  });
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
