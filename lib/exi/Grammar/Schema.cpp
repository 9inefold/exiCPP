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
#include <core/Common/EnumArray.hpp>
#include <core/Common/MMatch.hpp>
#include <core/Common/SmallVec.hpp>
#include <core/Common/STLExtras.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/Logging.hpp>
#include <core/Support/TrailingArray.hpp>
#include <exi/Basic/ExiOptions.hpp>
#include <exi/Grammar/Grammar.hpp>
#include <exi/Stream/StreamVariant.hpp>
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
# define CC __attribute__((preserve_none))
#else
# define CC
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

namespace {

/// A small log2 table for deducing bit counts. The maximum value a builtin
/// schema can have is 7, with `StartTagContent.{CM, PI}` with `SC` enabled.
alignas(16) static constexpr u8 SmallLog2[10] {0, 0, 1, 2, 2, 3, 3, 3, 3, 4};

static constexpr StringLiteral BIGrammarNames[] {
  "Document",
  "DocContent",
  "DocEnd",
  "StartTagContent",
  "ElementContent",
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
class DynBuiltinSchema final
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
  /// The grammar stack.
  SmallVec<GrammarT, 0> GStack;
  /// The pseudo grammar stack.
  BuiltinSchema::Grammar Current, Prev = Document;
  /// Current depth of SE to EE events.
  i64 SEDepth = 0;

  DynBuiltinSchema(const SmallVecImpl<EventTerm>& Terms) : 
   BaseT(Terms.size(), Terms.begin(), Terms.end()), Current(Document) {
  }

public:
  static Box<DynBuiltinSchema> New(const ExiOptions& Opts);

  EventTerm decode(ExiDecoder* D) override {
    tail_return this->getTermImpl(D);
  }

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

  ALWAYS_INLINE void pushGrammar(BuiltinSchema::Grammar New) {
    Prev = Current;
    Current = New;
  }

  MatchT decodeTerm(StreamReader& Strm, const int Start, unsigned At = 0) {
    const unsigned Offset = Info[Current].Offset;
    const auto& Code = Info[Current].Code;

    for (int Ix = Start, E = Code.Length; Ix < E; ++Ix) {
      if (Code.Data[Ix] == 0)
        /// The only level allowed to have zero elements is the first.
        continue;
      
      const u64 Bits = Code.Bits[Ix];
      const u64 Data = Strm->readBits64(Bits);
      At += Data;

      exi_invariant(Code.Data[Ix] != 0,
        "EventCode node not pruned!");
      const u64 CData = Code.Data[Ix] - 1;
      exi_invariant(Data <= CData);
      if (Data != CData)
        break;
    }

    const auto Term = BaseT::at(Offset + At);
    return MatchT(Term);
  }

  MatchT decodeTerm(StreamReader& Strm) {
    return this->decodeTerm(Strm, /*Start=*/0, /*At=*/0);
  }

  ALWAYS_INLINE MatchT decodeTerm(ExiDecoder* D) {
    auto& Strm = Get::Reader(D);
    return this->decodeTerm(Strm);
  }

  EventUID decodeTermGrammar(ExiDecoder* D) {
    exi_invariant(!GStack.empty());
    GrammarT G = GStack.back();

    auto& Strm = Get::Reader(D);
    const auto Ret = G->getTerm(Strm, G.getInt());
    if (Ret.is_ok())
      return *Ret;
    
    const MatchT M = this->decodeTerm(Strm, Ret.error(), 1);
    return EventUID::NewTerm(M.Data);
  }

  CC EventTerm getTermImpl(ExiDecoder* D) {
    LOG_EXTRA("Grammar: {}", GetGrammarName(Current));

    // Very rarely set. In most (or all?) cases it only happens once.
    if EXI_UNLIKELY(Current == Document) {
      // Document is always empty, and therefore never reads.
      this->pushGrammar(DocContent);
      return EventTerm::SD;
    }

    switch (Current) {
    case DocContent:
      tail_return this->handleDocContent(D);
    case DocEnd:
      tail_return this->handleDocEnd(D);
    case StartTagContent:
      tail_return this->handleStartTag(D);
    case ElementContent:
      tail_return this->handleElement(D);
    case Fragment:
      exi_unreachable("SC elements are currently unsupported");
    default:
      exi_unreachable("invalid state?");
    }
  }

  CC EventTerm handleDocContent(ExiDecoder* D) {
    using enum EventTerm;
    const auto M = this->decodeTerm(D);

    if (M.is(SE)) {
      this->pushGrammar(StartTagContent);
      ++SEDepth;
      return EventTerm::SE;
    } else if (M.is(DT, CM, PI))
      return M.Data;
    
    exi_unreachable("invalid DocContent");
  }

  CC GNU_ATTR(cold) EventTerm handleDocEnd(ExiDecoder* D) {
    using enum EventTerm;
    const auto M = this->decodeTerm(D);

    if (M.is(ED)) {
      exi_assert(SEDepth == 0, "invalid nesting");
      return EventTerm::ED;
    } else if (M.is(CM, PI))
      return M.Data;
    
    exi_unreachable("invalid DocEnd");
  }

  CC GNU_ATTR(hot) EventTerm handleStartTag(ExiDecoder* D) {
    using enum EventTerm;
    if EXI_UNLIKELY(GStack.empty())
      tail_return this->handleStartTagFirst(D);

    const auto Val = this->decodeTermGrammar(D);
    // TODO: Handle
    exi_unreachable("invalid StartTagContent");
  }

  CC GNU_ATTR(hot) EventTerm handleElement(ExiDecoder* D) {
    using enum EventTerm;
    if EXI_UNLIKELY(GStack.empty())
      tail_return this->handleElementFirst(D);
    
    const auto Val = this->decodeTermGrammar(D);
    // TODO: Handle
    exi_unreachable("invalid ElementContent");
  }

  /// Handles StartTag on an empty grammar stack.
  CC GNU_ATTR(cold) EventTerm handleStartTagFirst(ExiDecoder* D) {
    using enum EventTerm;
    const auto M = this->decodeTerm(D);

    if (M.is(EE))
      return this->handleEE(D);
    else if (M.is(SE))
      return this->handleSE(D);
    else if (M.is(AT, NS)) {
      this->pushGrammar(ElementContent);
      ++SEDepth;
      return M.Data;
    } else if (M.is(SC)) {
      this->pushGrammar(Fragment);
      return EventTerm::SC;
    } else if (M.is(CH, ER, CM, PI))
      return M.Data;
    
    exi_unreachable("invalid StartTagContent");
  }

  /// Handles Element on an empty grammar stack.
  CC GNU_ATTR(cold) EventTerm handleElementFirst(ExiDecoder* D) {
    using enum EventTerm;
    const auto M = this->decodeTerm(D);

    if (M.is(EE))
      return this->handleEE(D);
    else if (M.is(SE))
      return this->handleSE(D);
    else if (M.is(CH, ER, CM, PI))
      return M.Data;
    
    exi_unreachable("invalid ElementContent");
  }

  CC ALWAYS_INLINE EventTerm handleSE(ExiDecoder*) {
    this->pushGrammar(ElementContent);
    ++SEDepth;
    return EventTerm::SE;
  }

  CC EventTerm handleEE(ExiDecoder*) {
    --SEDepth;
    // TODO: Check this... I'm doing everything in my power to avoid a stack.
    this->pushGrammar(SEDepth > 0 ? Prev : DocEnd);
    exi_invariant(SEDepth >= 0, "invalid nesting");
    return EventTerm::EE;
  }
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

} // namespace `anonymous`

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
