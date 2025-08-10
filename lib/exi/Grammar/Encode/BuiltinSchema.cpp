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
#include <exi/Grammar/Grammar.hpp>
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

template <is_ordwriter_stream StrmT>
class INTERNAL_LINKAGE OrderedBuiltinSchema final
    : public BuiltinSchema,
      public TrailingArray<OrderedBuiltinSchema<StrmT>, EventTerm> {
  using enum BIGrammarState;
  using BuiltinSchema::State;

  using Get = Schema::Get<StrmT>;
  using BaseT = TrailingArray<OrderedBuiltinSchema, EventTerm>;
  using MatchT = MMatch<EventTerm, EventTerm>;
  using GrammarT = PointerIntPair<BuiltinGrammar*, 1, bool>;

  /// Contains info on the compressed grammars.
  BIInfoArray Info;
  /// The current event ID
  EventUID Event = EventUID::NewNull();
  /// The pseudo grammar stack.
  BIGrammarState Current = Document;
  /// ...

  OrderedBuiltinSchema(ArrayRef<EventTerm> Terms,
                       ArrayRef<BIInfo> Info) : 
   BaseT(Terms.size(), Terms.begin(), Terms.end()),
   Info(BIInfoArray::New(Info)) {
  }
  OrderedBuiltinSchema(OrderedEncoder&, const BIBuilder& BIB) : 
   OrderedBuiltinSchema(BIB.Terms, BIB.Info) {
  }

public:
  friend class BaseT::New;
  using InitT = typename BaseT::New;

  static Box<OrderedBuiltinSchema> New(OrderedEncoder* OE,
                                       const BIBuilder& BIB) {
    exi_invariant(BIB, "Builder must be initialized!");
    const unsigned Size = BIB.Terms.size();
    auto* TheSchema = InitT(Size).init(*OE, BIB);
    return Box<OrderedBuiltinSchema>(TheSchema);
  }
  template <is_exi_allocator Alloc>
  [[nodiscard]] static OrderedBuiltinSchema*
   New(OrderedEncoder* OE, const BIBuilder& BIB, Alloc& A) {
    exi_invariant(BIB, "Builder must be initialized!");
    const unsigned Size = BIB.Terms.size();
    return InitT(Size, A).init(*OE, BIB);
  }
  static Box<OrderedBuiltinSchema> New(BodyEncoder* BE,
                                       const ExiOptions& Opts) {
    if (auto* OE = dyn_cast_if_present<OrderedEncoder>(BE))
      return New(OE, BIBuilder::New(Opts));
    LOG_ERROR("Input was null or not an ordered encoder!");
    return nullptr;
  }

  ////////////////////////////////////////////////////////////////////////
  // Encoding

  ExiError encode(BodyEncoder* BE, const BaseEvent& Event,
                                   SimpleEventTerm Term) override {
    exi_expensive_invariant(isa<OrderedEncoder>(BE));
    return this->setEventImpl(static_cast<OrderedEncoder*>(BE), Event, Term);
  }

private:
  /// Encodes a simple event - one where grammar context isn't required.
  template <State S, typename EventT>
  ALWAYS_INLINE ExiError encodeEventS(OrderedEncoder* OE, const EventT& Event) {
    // ...
    return ExiError::OK;
  }
  
  /// Encodes a simple event - one where grammar context isn't required.
  template <State S, SimpleEventTerm Term>
  CC ExiError encodeEventS(ORDERED_ARGS) {
    return this->encodeEventS<S>(
      OE, event_cast<Term>(Event, K));
  }

  ALWAYS_INLINE void pushGrammar(State New) { Current = New; }

  /// ENTRY POINT - Dispatches common events.
  CC_INLINE ExiError setEventImpl(ORDERED_ARGS) {
    this->logEvent(Event, K);

    switch (Current) {
    case StartTagContent:
      //tail_return this->handleStartTag(ORDERED_NEXT);
      exi_todo("Implement StartTagContent");
    case ElementContent:
      //tail_return this->handleElement(ORDERED_NEXT);
      exi_todo("Implement ElementContent");
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
    switch (K) {
    case SimpleEventTerm::SE:
      // This should only be called once, at the start of processing.
      //exi_assert(GStack.empty() && Grammars.empty());
      exi_todo("DocContent: Implement SE");
      return ExiError::OK;
    case SimpleEventTerm::CM:
      tail_return encodeEventS<S, CM>(ORDERED_NEXT);
    case SimpleEventTerm::PI:
      tail_return encodeEventS<S, PI>(ORDERED_NEXT);
    case SimpleEventTerm::DT:
      tail_return encodeEventS<S, DT>(ORDERED_NEXT);
    default:
      exi_unreachable("invalid DocContent");
    }
  }

  CC ExiError handleDocEnd(ORDERED_ARGS) {
    using enum SimpleEventTerm;
    static constexpr State S = DocContent;
    switch (K) {
    case SimpleEventTerm::SE:
      // This should only be called once, at the start of processing.
      //exi_assert(GStack.empty() && Grammars.empty());
      exi_todo("DocContent: Implement SE");
      return ExiError::OK;
    case SimpleEventTerm::CM:
      tail_return encodeEventS<S, CM>(ORDERED_NEXT);
    case SimpleEventTerm::PI:
      tail_return encodeEventS<S, PI>(ORDERED_NEXT);
    default:
      exi_unreachable("invalid DocEnd");
    }
  }

  ////////////////////////////////////////////////////////////////////////
  // Event Handling

  ////////////////////////////////////////////////////////////////////////
  // Grammar

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
#else
  //ALWAYS_INLINE constexpr void logCurrentGrammar(ExiDecoder*) {}
  //ALWAYS_INLINE constexpr void logCurrentEvent() {}
  ALWAYS_INLINE constexpr void logEvent(const BaseEvent&, SimpleEventTerm) {}
#endif
  void anchor() override;
};

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
#endif // EXI_LOGGING

//===----------------------------------------------------------------===//
// Getters
//===----------------------------------------------------------------===//

namespace INTERNAL_NS(exi) {

template <is_ordwriter_stream StrmT>
class OrderedBISchemaFactory {
  BIBuilder Builder;
public:
  OrderedBISchemaFactory(const ExiOptions& Opts) : Builder(Opts) {
    Builder.init();
  }
  Box<BuiltinSchema> operator()(BodyEncoder* BE) const {
    if (auto* OE = dyn_cast<OrderedEncoder>(BE)) [[likely]]
      return OrderedBuiltinSchema<StrmT>::New(OE, Builder);
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
