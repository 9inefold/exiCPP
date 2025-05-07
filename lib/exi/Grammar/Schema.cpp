//===- exi/Grammar/Schema.cpp ---------------------------------------===//
//
// Copyright (C) 2024 Eightfold
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
/// This file defines the base for schemas.
///
//===----------------------------------------------------------------===//

#include <exi/Grammar/Schema.hpp>
#include <core/Common/DenseMap.hpp>
#include <core/Common/EnumArray.hpp>
#include <core/Common/MMatch.hpp>
#include <core/Common/SmallVec.hpp>
#include <core/Common/STLExtras.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/Logging.hpp>
#include <core/Support/TrailingArray.hpp>
#include <exi/Basic/ExiOptions.hpp>
#include <exi/Grammar/Grammar.hpp>
#include <exi/Stream/OrderedReader.hpp>
#include <fmt/ranges.h>
#include "SchemaGet.hpp"

using namespace exi;

#define DEBUG_TYPE "Schema"

#ifdef __GNUC__
# define GNU_ATTR(...) __attribute__((__VA_ARGS__))
#else
# define GNU_ATTR(...)
#endif

#if EXI_HAS_ATTR(preserve_none)
/// Preserves none.
# define CC __attribute__((preserve_none))
/// Preserves none, inlines the function when unavailable.
# define CC_INLINE __attribute__((preserve_none))
#else
/// Empty attribute.
# define CC
/// Inlines the function.
# define CC_INLINE ALWAYS_INLINE
#endif

#ifdef __clang__
/// Keep debug information clean when using clang.
# define INTERNAL_LINKAGE [[clang::internal_linkage]]
# define INTERNAL_NS exi
#else
# define INTERNAL_LINKAGE
# define INTERNAL_NS
#endif

//===----------------------------------------------------------------===//
// Built-in Grammar
//===----------------------------------------------------------------===//

/// The transitions for schemaless encodings are defined as the following.
/// If SC is not enabled, then the ChildContentItems for StartTagContent will
/// be (0.3) instead.
///
/// Document:
///   SD DocContent  					0
/// 
/// DocContent:
///   SE (*) DocEnd  					0
///   DT DocContent  					1.0
///   CM DocContent  					1.1.0
///   PI DocContent  					1.1.1
/// 
/// DocEnd:
///   ED            					0
///   CM DocEnd     					1.0
///   PI DocEnd     					1.1
/// 
/// StartTagContent:
///   EE  										0.0
///   AT (*) StartTagContent  0.1
///   NS StartTagContent  		0.2
///   SC Fragment  						0.3
///   ChildContentItems 		 (0.4)  
/// 
/// ElementContent:
///   EE  										0
///   ChildContentItems 		 (1.0)  
/// 
/// ChildContentItems (n.m):
///   SE (*) ElementContent  n. m
///   CH ElementContent  		 n.(m+1)
///   ER ElementContent  		 n.(m+2)
///   CM ElementContent  		 n.(m+3).0
///   PI ElementContent  		 n.(m+3).1
///

namespace INTERNAL_NS {

/// A small log2 table for deducing bit counts. The maximum value a builtin
/// schema can have is 7, with `StartTagContent.{CM, PI}` with `SC` enabled.
alignas(16) static constexpr u8 SmallLog2[10] {0, 0, 1, 2, 2, 3, 3, 3, 3, 4};

static constexpr StringLiteral BIGrammarNames[] {
  "Document",
  "DocContent",
  "DocEnd",
  "StartTag",
  "Element",
  "Fragment"
};

/// Small EventCode for use in `BIInfo`.
struct SEventCode {
  Array<u8, 3> Data = {};  // [x.y.z]
  Array<u8, 3> Bits = {};  // [[x].[y].[z]]
  i8 Length = 0;           // Number of pieces.
};

struct BIInfo {
  u8 Offset = 0;
  SEventCode Code = {};
};

// TODO: Implement...
class INTERNAL_LINKAGE DynBuiltinSchema final
    : public BuiltinSchema, 
      public TrailingArray<DynBuiltinSchema, EventTerm> {
  using enum BuiltinSchema::Grammar;
  class Builder;

  using BaseT = TrailingArray<DynBuiltinSchema, EventTerm>;
  using InfoT = EnumeratedArray<BIInfo, Grammar, Last, DocContent>;
  using MatchT = MMatch<EventTerm, EventTerm>;
  using GrammarT = PointerIntPair<BuiltinGrammar*, 1, bool>;

  /// Contains info on the compressed grammars.
  InfoT Info;
  /// The current event ID
  EventUID Event = EventUID::NewNull();
  /// The pseudo grammar stack.
  BuiltinSchema::Grammar Current = Document;
  /// The grammar stack.
  /// TODO: Profile...
  Vec<GrammarT> GStack;
  /// The generated grammars.
  DenseMap<SmallQName, BuiltinGrammar*> Grammars;

  DynBuiltinSchema(const SmallVecImpl<EventTerm>& Terms) : 
   BaseT(Terms.size(), Terms.begin(), Terms.end()) {
  }

public:
  static Box<DynBuiltinSchema> New(const ExiOptions& Opts);

  ////////////////////////////////////////////////////////////////////////
  // Decoding

  EventUID decode(ExiDecoder* D) override {
    tail_return this->getTermImpl(D);
  }

private:
  MatchT createDecodedTerm(unsigned At) {
    const unsigned Offset = Info[Current].Offset;
    const auto Term = BaseT::at(Offset + At);
    this->Event = EventUID::NewTerm(Term);
    return MatchT(Term);
  }

  MatchT decodeTerm(OrdReader& Strm, int Start, unsigned At = 0) {
    const unsigned Offset = Info[Current].Offset;
    const auto& Code = Info[Current].Code;

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

  MatchT decodeTerm(OrdReader& Strm) {
    return this->decodeTerm(Strm, /*Start=*/0, /*At=*/0);
  }

  ALWAYS_INLINE MatchT decodeTerm(ExiDecoder* D) {
    auto& Strm = Get::Reader(D);
    return this->decodeTerm(Strm);
  }  

  inline GrammarTerm getGrammarTerm(ExiDecoder* D) {
    exi_invariant(!GStack.empty());
    GrammarT G = GStack.back();

    auto& Strm = Get::Reader(D);
    return G->getTerm(Strm, G.getInt());
  }

  EventUID decodeTermGrammar(ExiDecoder* D) {
    const auto Ret = getGrammarTerm(D);
    if (Ret.is_ok()) {
      LOG_EXTRA("Grammar hit");
      this->Event = *Ret;
      return *Ret;
    }
    
    // LOG_EXTRA("Grammar miss: {}", Ret.error());
    auto& Strm = Get::Reader(D);
    const MatchT M = this->decodeTerm(Strm, 1, Ret.error());
    return this->Event;
  }

  ALWAYS_INLINE void pushGrammar(BuiltinSchema::Grammar New) {
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
      exi_unreachable("SC elements are currently unsupported");
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
    
    exi_unreachable("invalid DocContent");
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
    
    exi_unreachable("invalid DocEnd");
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
      tail_return this->handleEE(D);
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
      D->diagnose(Event.error());
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
      D->diagnose(Event.error());
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

    Event.setTerm(Cached ? ATQName : AT);
    return Event;
  }

  template <bool Cached = false>
  CC EventUID handleCH(ExiDecoder* D) {
    exi_invariant(!GStack.empty());
    const SmallQName Name = GStack.back()->getName();
    const auto Event = Get::DecodeValue(D, Name);

    if EXI_UNLIKELY(Event.is_err()) {
      D->diagnose(Event.error());
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
  void PrintGrammar(BuiltinSchema::Grammar G) const;

private:
  static StrRef GetGrammarName(Grammar G) {
    constexpr int BIMax = std::size(BIGrammarNames);
    const auto Ix = exi::to_underlying(G);
    if (Ix < BIMax && Ix >= 0)
      return BIGrammarNames[Ix];
    return "???"_str;
  }

#if EXI_LOGGING
  void logCurrentGrammar(ExiDecoder* D);
  void logCurrentEvent();
  void logEvent(EventTerm Term);
#else
  ALWAYS_INLINE constexpr void logCurrentGrammar(ExiDecoder*) {}
  ALWAYS_INLINE constexpr void logCurrentEvent() {}
  ALWAYS_INLINE constexpr void logEvent(EventTerm) {}
#endif
};

class DynBuiltinSchema::Builder {
public:
  SmallVec<EventTerm, 8> Terms;
  SmallVec<BIInfo, DynBuiltinSchema::InfoT::size()> Info;

private:
  ExiOptions::PreserveOpts Preserve;
  bool SelfContained;

  class EventCodeRTTI {
    SEventCode* C;
  public:
    EventCodeRTTI(SEventCode* EC) : C(EC) {}
    ~EventCodeRTTI() { Builder::CalculateLog(C); }
    SEventCode& operator*() { return *C; }
    SEventCode* operator->() { return C; }
  };

  static void CalculateLog(SEventCode* EC);

  EventCodeRTTI createBIInfo() {
    const u8 Offset = IntCast<u8>(Terms.size());
    Info.push_back({
      .Offset = Offset,
      .Code { .Length = 1 }
    });
    return &Info.back().Code;
  }

public:
  Builder(const ExiOptions& Opts) :
   Preserve(Opts.Preserve),
   SelfContained(Opts.SelfContained) {
  }

  static void Inc(SEventCode& C, i8 I = 1) {
    if EXI_LIKELY(C.Length)
      C.Data[C.Length - 1] += I;
    else
      LOG_WARN("'Inc' ran on empty EventCode.");
  }
  static void Next(SEventCode& C) {
    if EXI_LIKELY(C.Length < 3)
      ++C.Length;
    else
      LOG_WARN("'Next' ran on full EventCode.");
  }
  static void IncNext(SEventCode& C, i8 I = 1) {
    Builder::Inc(C, I);
    Builder::Next(C);
  }

  void init() {
    /*DocContent:*/ {
      LOG_EXTRA("DocContent:");
      auto C = createBIInfo();
      Terms.push_back(EventTerm::SE);
      Builder::Inc(*C);

      if (Preserve.DTDs) {
        Terms.push_back(EventTerm::DT);
        Builder::IncNext(*C);
        Builder::Inc(*C);
      }

      this->addCMPI(*C);
    }

    /*DocEnd:*/ {
      LOG_EXTRA("DocEnd:");
      auto C = createBIInfo();
      Terms.push_back(EventTerm::ED);
      Builder::Inc(*C);
      this->addCMPI(*C);
    }

    /*StartTagContent:*/ {
      LOG_EXTRA("StartTagContent:");
      auto C = createBIInfo();
      Terms.push_back(EventTerm::EE);
      Terms.push_back(EventTerm::AT);
      Builder::Next(*C);
      Builder::Inc(*C, 2);

      if (Preserve.Prefixes) {
        Terms.push_back(EventTerm::NS);
        Builder::Inc(*C);
      }

      if (SelfContained) {
        Terms.push_back(EventTerm::SC);
        Builder::Inc(*C);
      }

      this->addCCItems(*C);
    }

    /*ElementContent:*/ {
      LOG_EXTRA("ElementContent:");
      auto C = createBIInfo();
      Terms.push_back(EventTerm::EE);
      Builder::Inc(*C, 2);
      this->addCCItems(*C);
    }
  }

private:
  /// Adds CM/PI to the end of a grammar, if possible.
  void addCMPI(SEventCode& C) {
    if (!Preserve.Comments && !Preserve.PIs)
      return;
    Builder::IncNext(C);
    if (Preserve.Comments) {
      Terms.push_back(EventTerm::CM);
      Builder::Inc(C);
    }
    if (Preserve.PIs) {
      Terms.push_back(EventTerm::PI);
      Builder::Inc(C);
    }
  }

  /// Adds ChildContentItems.
  void addCCItems(SEventCode& C) {
    exi_assert(C.Length <= 2);
    C.Length = 2;

    Terms.push_back(EventTerm::SE);
    Terms.push_back(EventTerm::CH);
    Builder::Inc(C, 2);

    if (Preserve.DTDs) {
      Terms.push_back(EventTerm::ER);
      Builder::Inc(C);
    }

    this->addCMPI(C);
  }
};

} // namespace INTERNAL_NS

void DynBuiltinSchema::Builder::CalculateLog(SEventCode* EC) {
  exi_invariant(EC);
  exi_assert(EC->Length <= 3 && EC->Length >= 0);

  auto& Length = EC->Length;
  if (Length == 0)
    return;
  
  auto& Data = EC->Data;
  // If we have `[x.y.0]`, make it `[x.y].0`.
  if (Length == 3 && !Data[2]) {
    Length -= 1;
  }

  // If we have `[x.0.z]`, make it `[x.z].0`.
  if (Length >= 2 && !Data[1]) {
    Data[1] = Data[2];
    Data[2] = 0;
    Length -= 1;
  }

  // We can't remove the first level event code, as it's possible grammars will
  // extend the base values. This would lead to inaccuracies.

  // Calculate log for all elements.
  for (int Ix = 0; Ix < 3; ++Ix)
    EC->Bits[Ix] = SmallLog2[Data[Ix]];
}

Box<DynBuiltinSchema> DynBuiltinSchema::New(const ExiOptions& Opts) {
  Builder B(Opts);
  B.init();

  using Trailing = DynBuiltinSchema::BaseT;
  void* Raw = Trailing::New(B.Terms.size());
  auto* Schema = new (Raw) DynBuiltinSchema(B.Terms);

  exi_assert(B.Info.size() == Schema->Info.size());
  for (auto [Ix, BuiltinInfo] : exi::enumerate(Schema->Info))
    // Copy all our generated info.
    BuiltinInfo = B.Info[Ix];
  
  return Box<DynBuiltinSchema>(Schema);
}

void DynBuiltinSchema::dump() const {
  outs() << "Document[1] <@0>:\n"
         << "  SD      0\n\n";
  PrintGrammar(Grammar::DocContent);
  PrintGrammar(Grammar::DocEnd);
  PrintGrammar(Grammar::StartTagContent);
  PrintGrammar(Grammar::ElementContent);
  outs().flush();
}

void DynBuiltinSchema::PrintGrammar(BuiltinSchema::Grammar G) const {
  const StrRef Name = GetGrammarName(G);
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
void DynBuiltinSchema::logCurrentGrammar(ExiDecoder* D) {
  using enum raw_ostream::Colors;
  if (!hasDbgLogLevel(VERBOSE))
    // Don't do any work if log level is insufficient.
    return;
  
  const auto OldColor = dbgs().getColor();
  dbgs().changeColor(BRIGHT_WHITE) << '\n';

  dbgs() << "Grammar State: " << GetGrammarName(Current);
  const bool IsContent = mmatch(Current).is(StartTagContent, ElementContent);
  if EXI_LIKELY(IsContent && !GStack.empty()) {
    const SmallQName ID = GStack.back()->getName();
    auto [URI, Name] = Get::Idents(D).getQName(ID);
    if (URI.empty())
      dbgs() << format("[{}]", Name);
    else
      dbgs() << format("[{}:{}]", URI, Name);
  }

  dbgs() << '\n' << OldColor;
}

void DynBuiltinSchema::logCurrentEvent() {
  if EXI_UNLIKELY(!Event.hasTerm())
    return;
  this->logEvent(Event.getTerm());
}

void DynBuiltinSchema::logEvent(EventTerm Term) {
  LOG_INFO("> With {}: {}",
    get_event_name(Term),
    get_event_signature(Term)
  );
}
#endif // EXI_LOGGING

Box<BuiltinSchema> BuiltinSchema::New(const ExiOptions& Opts) {
  // exi_unreachable("TODO");
  return DynBuiltinSchema::New(Opts);
}

//===----------------------------------------------------------------===//
// Miscellaneous
//===----------------------------------------------------------------===//

void Schema::anchor() {}
void BuiltinSchema::anchor() {}
void DynamicSchema::anchor() {}
void CompiledSchema::anchor() {}

const char Schema::ID = 0;
const char BuiltinSchema::ID = 0;
const char DynamicSchema::ID = 0;
const char CompiledSchema::ID = 0;
