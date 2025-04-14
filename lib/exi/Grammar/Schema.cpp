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
#include <exi/Stream/StreamVariant.hpp>
#include <fmt/ranges.h>

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

/// Small EventCode for use in `BIInfo`.
struct SEventCode {
  Array<u8, 3> Data = {}; // [x.y.z]
  Array<u8, 3> Bits = {}; // [[x].[y].[z]]
  i8 Length = 0;          // Number of pieces.
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

  /// Contains info on the compressed grammars.
  InfoT Info;
  /// The pseudo grammar stack.
  BuiltinSchema::Grammar Current, Prev = Document;
  /// Current depth of SE to EE events.
  i64 SEDepth = 0;

  DynBuiltinSchema(const SmallVecImpl<EventTerm>& Terms) : 
   BaseT(Terms.size(), Terms.begin(), Terms.end()), Current(Document) {
  }

public:
  static Box<DynBuiltinSchema> New(const ExiOptions& Opts);

  EventTerm getTerm(StreamReader& Strm) override {
    tail_return this->getTermImpl(Strm);
  }

  void dump() const override {
    outs() << "Document[1] <@0>:\n"
           << "  SD      0\n\n";
    PrintGrammar(Grammar::DocContent, "DocContent");
    PrintGrammar(Grammar::DocEnd, "DocEnd");
    PrintGrammar(Grammar::StartTagContent, "StartTagContent");
    PrintGrammar(Grammar::ElementContent, "ElementContent");
    outs().flush();
  }

  void PrintGrammar(BuiltinSchema::Grammar G, StrRef Name) const {
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

    PrintEvent(0);
    for (int IC = 0; IC < Code.Length; ++IC) {
      const int ICMax = i32(Code.Data[IC]) - 1;
      for (int Ix = 1; Ix < ICMax; ++Ix)
        PrintEvent(Ix);
      
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
      PrintEvent(0);
    }

    outs() << '\n';
  }

private:
  ALWAYS_INLINE void pushGrammar(BuiltinSchema::Grammar New) {
    Prev = Current;
    Current = New;
  }

  MatchT decodeTerm(StreamReader& Strm) {
    const unsigned Offset = Info[Current].Offset;
    const auto& Code = Info[Current].Code;
    // EventTerm* const Base = BaseT::data() + Offset;

    unsigned At = 0;
    for (int Ix = 0, E = Code.Length; Ix < E; ++Ix) {
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

  CC EventTerm getTermImpl(StreamReader& Strm) {
    // Very rarely set. In most (or all?) cases it only happens once.
    if EXI_UNLIKELY(Current == Document) {
      // Document is always empty, and therefore never reads.
      this->pushGrammar(DocContent);
      return EventTerm::SD;
    }

    switch (Current) {
    case DocContent:
      tail_return this->handleDocContent(Strm);
    case DocEnd:
      tail_return this->handleDocEnd(Strm);
    case StartTagContent:
      tail_return this->handleStartTag(Strm);
    case ElementContent:
      tail_return this->handleElement(Strm);
    case Fragment:
      exi_unreachable("SC elements are currently unsupported");
    default:
      exi_unreachable("invalid state?");
    }
  }

  CC EventTerm handleDocContent(StreamReader& Strm) {
    using enum EventTerm;
    const auto M = this->decodeTerm(Strm);

    if (M.is(SE)) {
      this->pushGrammar(StartTagContent);
      ++SEDepth;
      return EventTerm::SE;
    } else if (M.is(DT, CM, PI))
      return M.Data;
    
    exi_unreachable("invalid DocContent");
  }

  CC GNU_ATTR(cold) EventTerm handleDocEnd(StreamReader& Strm) {
    using enum EventTerm;
    const auto M = this->decodeTerm(Strm);

    if (M.is(ED)) {
      exi_assert(SEDepth == 0, "invalid nesting");
      return EventTerm::ED;
    } else if (M.is(CM, PI))
      return M.Data;
    
    exi_unreachable("invalid DocEnd");
  }

  CC GNU_ATTR(hot) EventTerm handleStartTag(StreamReader& Strm) {
    using enum EventTerm;
    const auto M = this->decodeTerm(Strm);

    if (M.is(EE))
      return this->handleEE(Strm);
    else if (M.is(SE))
      return this->handleSE(Strm);
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

  CC GNU_ATTR(hot) EventTerm handleElement(StreamReader& Strm) {
    using enum EventTerm;
    const auto M = this->decodeTerm(Strm);

    if (M.is(EE))
      return this->handleEE(Strm);
    else if (M.is(SE))
      return this->handleSE(Strm);
    else if (M.is(CH, ER, CM, PI))
      return M.Data;
    
    exi_unreachable("invalid ElementContent");
  }

  template <usize M>
  CC ALWAYS_INLINE EventTerm childContentItems(StreamReader& Strm, const u64 Val) {
    exi_unreachable("invalid ChildContentItems");
  }

  CC ALWAYS_INLINE EventTerm handleSE(StreamReader&) {
    this->pushGrammar(ElementContent);
    ++SEDepth;
    return EventTerm::SE;
  }

  CC EventTerm handleEE(StreamReader&) {
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

  // If we have `[0.y.z]`, make it `[y.z].0`.
  // If we have `[0.y].0`, make it `[y].0.0`.
  if (Length >= 1 && !Data[0]) {
    Data[0] = Data[1];
    Data[1] = Data[2];
    Data[2] = 0;
    Length -= 1;
  }

  /// Calculate log for all elements.
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

Box<BuiltinSchema> BuiltinSchema::GetSchema(bool SelfContained) {
  exi_unreachable("why........");
}

Box<BuiltinSchema> BuiltinSchema::New(const ExiOptions& Opts) {
  // exi_unreachable("TODO");
  return DynBuiltinSchema::New(Opts);
}

void Schema::anchor() {}
void BuiltinSchema::anchor() {}
void DynamicSchema::anchor() {}
void CompiledSchema::anchor() {}

const char Schema::ID = 0;
const char BuiltinSchema::ID = 0;
const char DynamicSchema::ID = 0;
const char CompiledSchema::ID = 0;
