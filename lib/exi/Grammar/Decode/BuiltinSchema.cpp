//===- exi/Grammar/Decode/BuiltinSchema.cpp -------------------------===//
//
// Copyright (C) 2024-2025 Eightfold
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

#include <exi/Grammar/DecoderSchema.hpp>
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
#include <exi/Decode/Grammar.hpp>
#include <exi/Grammar/BIBuilder.hpp>
#include <exi/Stream/OrderedReader.hpp>
#include <fmt/ranges.h>
#include "SchemaGet.hpp"

using namespace exi;
using namespace exi::decode;

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

/// Builtin event decoding works like so:
/// 1. Dispatch to the current transition function
///   - Handle the most common transition types, or
///   - Fallthrough to the uncommon states (Docstart/end/content)
/// 2. Decode the TERM, either directly or through a grammar
///   a. If through a grammar:
///     - If it is specialized TERM code, RETURN it, otherwise
///     - Save the specialized first-level code and GOTO b
///   b. If direct:
///     - Load the code generated for the current transition
///     - If the code is equal to the max, load the next code, otherwise repeat
///     - Add the codes together to get the TERM offset and RETURN it
/// 3. Handle the TERM for the specific grammar
///   a. If simple/stateless:
///     - Determine the next transition
///     - RETURN the TERM
///   b. If stateful:
///     - Jump to the TERM specific handler
///     - If uncached, decode the TERM data, then add to grammar
///     - If element, create or load the next grammar
///     - Construct the compound TERM and RETURN it
///
/// Builtin schemas are structured like:
/// [Transition LUT][State][Dynamic Data...][TERMS...]
///
/// The LUT and terms are constructed dynamically based on current options using
/// the BIBuilder. The terms are in a dynamically sized array following the class.
///
/// Grammars are stored in a densely packed map, indexed with numeric QNames.
/// The Grammar stack stores a compressed pair of [Grammar, Start-Or-Element].
/// Start-Or-Element is used to modify the behaviour of grammar lookups.

namespace INTERNAL_NS(exi) {

//===----------------------------------------------------------------===//
// Ordered Encoding
//===----------------------------------------------------------------===//

template <class StrmT>
class INTERNAL_LINKAGE OrderedBuiltinSchema final
    : public BuiltinSchema,
      public TrailingArray<OrderedBuiltinSchema<StrmT>, EventTerm> {
  using enum BIGrammarState;
  using BuiltinSchema::State;

  using Get = Schema::Get<ExiDecoder, StrmT>;
  using BaseT = TrailingArray<OrderedBuiltinSchema, EventTerm>;
  using MatchT = MMatch<EventTerm, EventTerm>;
  using GrammarT = PointerIntPair<decode::BuiltinGrammar*, 1, bool>;

  /// Contains info on the compressed grammars.
  BIInfoArray Info;
  /// The current event ID
  EventUID Event = EventUID::NewNull();
  /// The pseudo grammar stack.
  BIGrammarState Current = Document;
  /// The grammar stack.
  /// TODO: Profile...
  Vec<GrammarT> GStack;
  /// The generated grammars.
  DenseMap<SmallQName, BuiltinGrammar*> Grammars;

  OrderedBuiltinSchema(ArrayRef<EventTerm> Terms,
                       ArrayRef<BIInfo> Info) : 
   BaseT(Terms.size(), Terms.begin(), Terms.end()),
   Info(BIInfoArray::New(Info)) {
  }
  OrderedBuiltinSchema(const BIBuilder& B) : 
   OrderedBuiltinSchema(B.terms(), B.info()) {
  }

public:
  friend class BaseT::New;
  using InitT = typename BaseT::New;

  static Box<OrderedBuiltinSchema> New(const BIBuilder& BIB) {
    exi_invariant(BIB, "Builder must be initialized!");
    const unsigned Size = BIB.trailing();
    auto* TheSchema = InitT(Size).init(BIB);
    return Box<OrderedBuiltinSchema>(TheSchema);
  }
  template <is_exi_allocator Alloc>
  [[nodiscard]] static OrderedBuiltinSchema* New(const BIBuilder& BIB, Alloc& A) {
    exi_invariant(BIB, "Builder must be initialized!");
    const unsigned Size = BIB.trailing();
    return InitT(Size, A).init(BIB);
  }
  static Box<OrderedBuiltinSchema> New(const ExiOptions& Opts) {
    return New(BIBuilder::New(Opts));
  }

  ////////////////////////////////////////////////////////////////////////
  // Decoding

  EventUID decode(ExiDecoder* D) override {
    tail_return this->getTermImpl(D);
  }

private:
  /// TODO: Array indexing is faster with hardcoded offsets.
  /// Add a template parameter to do this efficiently.
  MatchT createDecodedTerm(unsigned At) {
    const unsigned Offset = Info[Current].Offset;
    const auto Term = BaseT::at(Offset + At);
    this->Event = EventUID::NewTerm(Term);
    return MatchT(Term);
  }

  GNU_ATTR(hot) MatchT decodeTerm(StrmT* Strm, int Start, unsigned At = 0) {
    SEventCode Code = Info[Current].Code;
    if (Code.Length && Start == 0) {
      /// The only level allowed to have zero elements is the first.
      if (Code.Data[0] == 0)
        Start = 1;
    } else if (Start == 1 && Code.Data[0]) {
      const u64 CData = Code.Data[0] - 1;
      if (At != CData)
        return this->createDecodedTerm(At);
    }

    for (int Ix = Start, E = Code.Length; Ix < E; ++Ix) {
      const u64 Bits = Code.Bits[Ix];
      const u64 Data = *Strm->readBits64(Bits);
      At += Data;

      LOG_EXTRA("Code[{}]: @{}:{}", Ix, Bits, Data);
      exi_invariant(Code.Data[Ix] != 0,
        "EventCode node not pruned!");

      const u64 CData = Code.Data[Ix] - 1;
      exi_invariant(Data <= CData);
      if (Data != CData)
        break;
    }

    return this->createDecodedTerm(At);
  }

  MatchT decodeTerm(StrmT* Strm) {
    return this->decodeTerm(Strm, /*Start=*/0, /*At=*/0);
  }

  ALWAYS_INLINE MatchT decodeTerm(ExiDecoder* D) {
    auto* Strm = Get::Reader(D);
    return this->decodeTerm(Strm);
  }  

  inline GrammarTerm getGrammarTerm(ExiDecoder* D) {
    exi_invariant(!GStack.empty());
    GrammarT G = GStack.back();
    return G->getTerm<StrmT>(
      Get::Reader(D), G.getInt());
  }

  EventUID decodeTermGrammar(ExiDecoder* D) {
    const auto Ret = getGrammarTerm(D);
    if (Ret.is_ok()) {
      LOG_EXTRA("Grammar hit");
      this->Event = *Ret;
      return *Ret;
    }

    auto* Strm = Get::Reader(D);
    const MatchT M = this->decodeTerm(Strm, 1, Ret.error());
    return this->Event;
  }

  ALWAYS_INLINE void pushGrammar(State New) {
    Current = New;
  }

  CC_INLINE EventUID getTermImpl(ExiDecoder* D) {
    this->logCurrentGrammar(D);

    switch (Current) {
    case StartTagContent:
      tail_return this->handleStartTag(D);
    case ElementContent:
      tail_return this->handleElement(D);
    case Fragment:
      exi_unimplemented("SC elements are currently unsupported");
    default:
      tail_return this->getDocTerm(D);
    }
  }

  CC_INLINE GNU_ATTR(cold) EventUID getDocTerm(ExiDecoder* D) {
    switch (Current) {
    case Document:
      // Very rarely set. It only happens once at the start.
      tail_return this->handleDocument(D);
    case DocContent:
      tail_return this->handleDocContent(D);
    case DocEnd:
      tail_return this->handleDocEnd(D);
    default:
      exi_unreachable("invalid state?");
    }
  }

  ////////////////////////////////////////////////////////////////////////
  // States

  /// Invokes `EventUID::NewTerm`.
  ALWAYS_INLINE constexpr EventUID NewTerm(EventTerm Term) {
    this->Event = EventUID::NewTerm(Term);
    return Event;
  }

  ALWAYS_INLINE constexpr bool MatchTerm(auto...Terms) {
    return MatchT(this->Event.getTerm()).is(Terms...);
  }

  CC_INLINE GNU_ATTR(cold) EventUID handleDocument(ExiDecoder*) {
    // Document is always empty, and therefore never reads.
    this->pushGrammar(DocContent);
    this->logEvent(EventTerm::SD);
    return NewTerm(EventTerm::SD);
  }

  CC EventUID handleDocContent(ExiDecoder* D) {
    using enum EventTerm;
    const auto M = this->decodeTerm(D);
    this->logEvent(M.Data);

    if (M.is(SE)) {
      // This should only be called once, at the start of processing.
      exi_assert(GStack.empty() && Grammars.empty());
      tail_return this->handleSE</*IsRoot=*/true>(D);
    } else if (M.is(DT, CM, PI))
      return NewTerm(M.Data);
    
    exi_guardrail("invalid DocContent");
  }

  CC EventUID handleDocEnd(ExiDecoder* D) {
    using enum EventTerm;
    const auto M = this->decodeTerm(D);
    this->logEvent(M.Data);

    if (M.is(ED)) {
      exi_relassert(GStack.empty(), "invalid nesting");
      return NewTerm(EventTerm::ED);
    } else if (M.is(CM, PI))
      return NewTerm(M.Data);
    
    exi_guardrail("invalid DocEnd");
  }

  /// Handles StartTag unique elements.
  CC GNU_ATTR(hot) EventUID handleStartTag(ExiDecoder* D) {
    using enum EventTerm;
    this->decodeTermGrammar(D);
    const EventTerm Term = this->Event.getTerm();
    this->logEvent(Term);

    switch (Term) {
    case SE:
      // TODO: Early dispatch... check this.
      tail_return this->handleChildContent<true>(D);
    case EE:
      tail_return this->handleEEStartTag(D);
    case AT:
      tail_return this->handleAT(D);
    case ATQName:
      // GStack.back()->dump(D);
      tail_return this->handleATQName(D);
    case NS:
      return NewTerm(Term);
    case SC:
      this->pushGrammar(Fragment);
      return NewTerm(EventTerm::SC);
    default:
      tail_return this->handleSharedContent<true>(D);
    }
  }

  /// Forwards to shared content.
  CC GNU_ATTR(hot) EventUID handleElement(ExiDecoder* D) {
    using enum EventTerm;
    this->decodeTermGrammar(D);
    this->logCurrentEvent();
    tail_return this->handleSharedContent<false>(D);
  }

  /// Events shared between StartTag and Element.
  template <bool IsStart>
  CC EventUID handleSharedContent(ExiDecoder* D) {
    using enum EventTerm;
    const EventTerm Term = Event.getTerm();

    switch (Term) {
    case EE:
      tail_return this->handleEE<IsStart>(D);
    case SEQName:
      // SE(qname) events are cached.
      tail_return this->handleSEQName(D);
    case CHExtern:
      tail_return this->handleCH<true>(D);
    default:
      tail_return this->handleChildContent<IsStart>(D);
    }
  }

  /// Events under the ChildContentItems macro.
  template <bool IsStart>
  CC_INLINE EventUID handleChildContent(ExiDecoder* D) {
    static constexpr const char* UnreachableMsg
      = IsStart ? "invalid StartTagContent" : "invalid ElementContent";
    using enum EventTerm;
    const EventTerm Term = Event.getTerm();
    
    switch (Term) {
    case SE:
      // SE(*) events can only be uncached.
      tail_return this->handleSE(D);
    case CH:
      tail_return this->handleCH(D);
    case ER:
    case CM:
    case PI:
      this->pushGrammar(ElementContent);
      return NewTerm(Term);
    default:
      exi_unreachable(UnreachableMsg);
    }
  }

  ////////////////////////////////////////////////////////////////////////
  // Event Handling

  template <bool IsRoot = false>
  CC EventUID handleSE(ExiDecoder* D) {
    const auto Event = Get::DecodeQName(D);
    if EXI_UNLIKELY(Event.is_err()) {
      Diagnose(Event);
      return EventUID::NewNull();
    }

    this->Event = *Event;
    tail_return this->handleSEQName</*KnownCached=*/IsRoot>(D);
  }

  template <bool KnownCached = true>
  CC EventUID handleSEQName(ExiDecoder* D) {
    using enum EventTerm;
    exi_invariant(Event.hasQName());
    auto [G, IsCached] = loadGrammar(D, Event.Name);

    if constexpr (!KnownCached)
      this->addTerm<SEQName>(Event);
    this->pushGrammar(StartTagContent);

    Event.setTerm(IsCached ? SEQName : SE);
    GStack.emplace_back(G, /*IsStart=*/true);
    return Event;
  }

  /// Since element grammars always have single term EE event codes, we don't
  /// need to add it to the grammar.
  template <bool IsStart = false>
  CC EventUID handleEE(ExiDecoder*) {
    exi_invariant(!GStack.empty(), "invalid nesting");
    if constexpr (!IsStart)
      this->addQNameToEvent();
    GStack.pop_back();

    if EXI_LIKELY(!GStack.empty()) {
      this->pushGrammar(ElementContent);
      GStack.back().setInt(false);
    } else
      this->pushGrammar(DocEnd);
    
    return Event;
  }

  CC_INLINE EventUID handleEEStartTag(ExiDecoder* D) {
    using enum EventTerm;
    if EXI_UNLIKELY(!Event.hasQName()) {
      this->addQNameToEvent();
      // Cache event for current grammar.
      GStack.back()->addTerm(Event, /*IsStart=*/true);
    }
    tail_return this->handleEE</*IsStart=*/true>(D);
  }

  template <bool Cached = false>
  CC EventUID handleAT(ExiDecoder* D) {
    const auto Event = Get::DecodeQName(D);
    if EXI_UNLIKELY(Event.is_err()) {
      Diagnose(Event);
      return EventUID::NewNull();
    }

    this->Event = *Event;
    tail_return this->handleATQName<Cached>(D);
  }

  template <bool Cached = true>
  CC EventUID handleATQName(ExiDecoder* D) {
    using enum EventTerm;
    exi_invariant(Event.hasQName());
    if constexpr (!Cached)
      this->addTerm<ATQName>(Event);
    // TODO: xsi:type
    Event.setTerm(Cached ? ATQName : AT);
    return Event;
  }

  template <bool Cached = false>
  CC EventUID handleCH(ExiDecoder* D) {
    exi_invariant(!GStack.empty());
    const SmallQName Name = GStack.back()->getName();
    const auto Event = Get::DecodeValue(D, Name);

    if EXI_UNLIKELY(Event.is_err()) {
      Diagnose(Event);
      return EventUID::NewNull();
    }

    this->Event = *Event;
    this->Event.Name = Name;
    tail_return this->handleCHValue<Cached>(D);
  }

  template <bool Cached = true>
  CC EventUID handleCHValue(ExiDecoder* D) {
    using enum EventTerm;
    exi_invariant(Event.hasQName());
    if constexpr (!Cached) {
      constexpr auto E = EventUID::NewNull();
      this->addTerm<CHExtern>(E);
    }
    
    this->pushGrammar(ElementContent);
    GStack.back().setInt(false);

    Event.setTerm(Cached ? CHExtern : CH);
    return Event;
  }

  ////////////////////////////////////////////////////////////////////////
  // Grammar

  ALWAYS_INLINE bool isStart() const {
    return (Current == StartTagContent);
  }

  template <EventTerm Term>
  EXI_INLINE void addTerm(EventUID Event) {
    // If it's not the root element, it shouldn't be empty.
    exi_invariant(!GStack.empty());
    Event.setTerm(Term);
    GStack.back()->addTerm(Event, isStart());
  }

  /// Uses the current grammar's QName as the event's.
  ALWAYS_INLINE void addQNameToEvent() {
    exi_invariant(!GStack.empty());
    this->Event.Name = GStack.back()->getName();
  }

  /// Returns `[Grammar, Cached]`.
  std::pair<BuiltinGrammar*, bool>
   loadGrammar(ExiDecoder* D, SmallQName Name) {
    if (auto* G = Grammars.lookup(Name))
      return {G, true};
    // Cache miss
    auto* G = this->makeGrammar(D, Name);
    return {G, false};
  }

  BuiltinGrammar* makeGrammar(ExiDecoder* D, SmallQName Name) {
    auto& BP = Get::BP(D);
    auto* G = new (BP) BuiltinGrammar(Name);
    auto [It, DidEmplace] = Grammars.try_emplace(Name, G);
    exi_invariant(DidEmplace, "grammar already added");
    return G;
  }

  EXI_INLINE BuiltinGrammar* getGrammar(SmallQName Name) const {
    return Grammars.at(Name);
  }

  ////////////////////////////////////////////////////////////////////////
  // Printing

public:
  void dump() const override;
  void PrintGrammar(State G) const;

private:
#if EXI_LOGGING
  EXI_PRESERVE_CALLSITE void logCurrentGrammar(ExiDecoder* D);
  EXI_PRESERVE_CALLSITE void logCurrentEvent();
  EXI_PRESERVE_CALLSITE void logEvent(EventTerm Term);
#else
  ALWAYS_INLINE constexpr void logCurrentGrammar(ExiDecoder*) {}
  ALWAYS_INLINE constexpr void logCurrentEvent() {}
  ALWAYS_INLINE constexpr void logEvent(EventTerm) {}
#endif
  void anchor() override;
};

//===----------------------------------------------------------------===//
// Channel Encoding (soon)
//===----------------------------------------------------------------===//

// ...

} // namespace INTERNAL_NS

template<> void OrderedBuiltinSchema<BitReader>::anchor() {}
template<> void OrderedBuiltinSchema<ByteReader>::anchor() {}

//===----------------------------------------------------------------===//
// Logging
//===----------------------------------------------------------------===//

template <class StrmT>
void OrderedBuiltinSchema<StrmT>::dump() const {
  outs() << "Document[1] <@0>:\n"
         << "  SD      0\n\n";
  PrintGrammar(State::DocContent);
  PrintGrammar(State::DocEnd);
  PrintGrammar(State::StartTagContent);
  PrintGrammar(State::ElementContent);
  outs().flush();
}

template <class StrmT>
void OrderedBuiltinSchema<StrmT>::PrintGrammar(State G) const {
  const StrRef Name = get_state_name(G);
  auto [Off, Code] = Info[G];
  const EventTerm* Base = BaseT::data() + Off;

  /*Format*/ {
    const int MaxIx = (Code.Length - 1);
    outs() << Name << '[';
    for (int Ix = 0; Ix < MaxIx; ++Ix)
      outs() << format("{}.", Code.Data[Ix]);
    outs() << format("{}] <", Code.Data[MaxIx]);
    
    for (int Ix = 0; Ix < MaxIx; ++Ix)
      outs() << format("@{}, ", Code.Bits[Ix]);
    outs() << format("@{}>:\n", Code.Bits[MaxIx]);
  }

  if (Code.Length == 0) {
    outs() << '\n';
    return;
  }

  SmallStr<8> Pre;
  int At = 0;

  auto PrintEvent = [&] (int Ix) {
    exi_assert(IntCast<unsigned>(At) < BaseT::size());
    StrRef Name = get_event_fullname(Base[At++]);
    outs() << "  "
      << format("{: <8}", Name)
      << format("{}{}", Pre, Ix) << '\n';
  };

  for (int IC = 0; IC < Code.Length; ++IC) {
    if (Code.Data[IC] == 0) {
      Pre.append("0.");
      continue;
    }

    PrintEvent(0);
    int ICMax = i32(Code.Data[IC]) - 1;
    for (int Ix = 1; Ix < ICMax; ++Ix)
      PrintEvent(Ix);
    
    ICMax = std::max(ICMax, 0);
    if (IC + 1 == Code.Length) {
      if (ICMax >= 1)
        PrintEvent(ICMax);
      break;
    } else if (IC + 2 == Code.Length) {
      if (Code.Data[IC + 1] <= 1) {
        PrintEvent(ICMax);
        break;
      }
    }

    wrap_stream(Pre) << format("{}.", ICMax);
  }

  outs() << '\n';
}

#if EXI_LOGGING
template <class StrmT>
EXI_PRESERVE_CALLSITE void OrderedBuiltinSchema<StrmT>::logCurrentGrammar(ExiDecoder* D) {
  using enum raw_ostream::Colors;
  if (!hasDbgLogLevel(VERBOSE))
    // Don't do any work if log level is insufficient.
    return;
  
  const auto OldColor = dbgs().getColor();
  dbgs().changeColor(BRIGHT_WHITE) << '\n';

  dbgs() << "State: " << get_state_name(Current);
  const bool IsContent = mmatch(Current).is(StartTagContent, ElementContent);
  if EXI_LIKELY(IsContent && !GStack.empty()) {
    const SmallQName ID = GStack.back()->getName();
    auto [URI, Name] = Get::Strings(D).getQName(ID);
    if (URI.empty())
      dbgs() << format("[{}]", Name);
    else
      dbgs() << format("[{}:{}]", URI, Name);
  }

  dbgs() << '\n' << OldColor;
}

template <class StrmT>
EXI_PRESERVE_CALLSITE void OrderedBuiltinSchema<StrmT>::logCurrentEvent() {
  if EXI_UNLIKELY(!Event.hasTerm())
    return;
  this->logEvent(Event.getTerm());
}

template <class StrmT>
EXI_PRESERVE_CALLSITE void OrderedBuiltinSchema<StrmT>::logEvent(EventTerm Term) {
  LOG_INFO("> With {}: {}",
    get_event_name(Term),
    get_event_signature(Term)
  );
}
#endif // EXI_LOGGING

//===----------------------------------------------------------------===//
// Getters
//===----------------------------------------------------------------===//

static Box<BuiltinSchema> NewChanneled(const ExiOptions& Opts) {
  exi_todo("channel readers are currently unsupported!");
}

Box<BuiltinSchema> BuiltinSchema::New(const ExiOptions& Opts) {
  switch (Opts.Alignment) {
  case AlignKind::BitPacked:
    return OrderedBuiltinSchema<BitReader>::New(Opts);
  case AlignKind::BytePacked:
    return OrderedBuiltinSchema<ByteReader>::New(Opts);
  case AlignKind::PreCompression:
    return NewChanneled(Opts);
  case AlignKind::None:
    LOG_ERROR("AlignKind cannot be None!");
    return nullptr;
  }
  exi_unreachable("invalid alignment!");
}
