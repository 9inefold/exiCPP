//===- exi/Basic/XMLCompare.cpp -------------------------------------===//
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
/// This file implements the XMLCompare class.
///
//===----------------------------------------------------------------===//

#include <exi/Basic/XMLCompare.hpp>
#include <core/Common/SmallVec.hpp>
#include <core/Common/StringExtras.hpp>
#include <core/Common/STLExtras.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/Logging.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/XML.hpp>
#include <exi/Encode/DTDParser.hpp>
#include <Common/StrRef-inl.hpp>
#include <bitset>

#define DEBUG_TYPE "XMLCompare"
#if !EXI_DEV_ONLY
CLANG_IGNORED("-Wunused-private-field")
#endif

using namespace exi;

/// Contains data on the kind of nodes allowed.
using NodeKindBitset = std::bitset<usize(NodeKind::node_last)>;
/// Contains: Document, Element, Data, and CDATA.
static constexpr NodeKindBitset kBitsetBasic = 0b0000'1111;
/// Contains: Document, Element, Data, CDATA, Comment, DTD, and PI.
static constexpr NodeKindBitset kBitsetDefault = 0b1101'1111;

static NodeKindBitset MakeBitset(ExiOptions::PreserveOpts Opts) {
  NodeKindBitset B = kBitsetBasic;
  // Warn on Preserve.Prefixes == false?
  if (Opts.Comments)
    B.set(NodeKind::node_comment, true);
  if (Opts.DTDs)
    B.set(NodeKind::node_doctype, true);
  if (Opts.PIs)
    B.set(NodeKind::node_pi, true);
  return B;
}

/// Type used to filter characters in `StrRef`.
using filter_t = StrRef::filter_t;
/// Filters newlines from text.
static constexpr auto kNewline = filter_t::FromChars("\n\r\v\f");
/// Filters whitespace from text.
static constexpr auto kWhitespace = filter_t::FromChars(" \t\n\r\v\f");

static StrRef GetTrimmedToken(StrRef& Data) {
  StrRef Out;
  std::tie(Out, Data) = getToken(Data, kNewline);
  return Out.trim(kWhitespace);
}

static StrRef GetNextToken(StrRef& Data) {
  StrRef Out = GetTrimmedToken(Data);
  while (Out.empty()) {
    if (Data.empty())
      return "";
    Out = GetTrimmedToken(Data);
  }
  return Out;
}

#if 0
static void WriteStringWithNoWS(StrRef S, raw_ostream& Out) {
  for (usize Ix = 0; Ix < S.size(); ++Ix) {
    const char C = S[Ix];
    if (isPrint(C))
      Out << C;
    else {
      switch (C) {
      case u8('\a'):
        Out << "\\a";
        break;
      case u8('\b'):
        Out << "\\b";
        break;
      case u8('\f'):
        Out << "\\f";
        break;
      case u8('\n'):
        Out << "\\n";
        break;
      case u8('\r'):
        if (S[Ix + 1] != '\n')
          Out << "\\r";
        break;
      case u8('\t'):
        Out << "\\t";
        break;
      case u8('\v'):
        Out << "\\v";
        break;
      default:
        Out << '\\' << hexdigit(C >> 4) << hexdigit(C & 0x0F);
      }
    }
  }
}

static String WriteDTDDifferences(StrRef LHS, StrRef RHS) {
  String Out;
  Out.reserve(LHS.size() + RHS.size() + 4);
  raw_string_ostream OS(Out);
  OS << "\n\t";
  WriteStringWithNoWS(LHS, OS);
  OS << "\n\t";
  WriteStringWithNoWS(RHS, OS);
  return Out;
}

static bool CompareInlDTD(StrRef LHS, StrRef RHS) {
  static constexpr auto kFilter
    = StrRef::filter_t::FromChars(
      DTDParser::kDelimiter);
  SmallVec<StrRef, 16> LHSVec, RHSVec;
  exi::SplitString(LHS, LHSVec, kFilter);
  exi::SplitString(RHS, RHSVec, kFilter);
  return LHSVec == RHSVec;
}

static bool DebugCompareDTDs(const DoctypeEvent& LHS, const DoctypeEvent& RHS) {
  if (LHS.Kind != RHS.Kind) {
    LOG_WARN("Different DOCTYPE types: {}, {}",
             get_doctype_name(LHS.Kind), get_doctype_name(RHS.Kind));
    return false;
  }

  auto LogDifferences = [&](StrRef Name, int N) {
#if EXI_DEBUG
    StrRef Split = Name.empty() ? "" : " ";
    String Diff = WriteDTDDifferences(LHS[N], RHS[N]);
    LOG_WARN("Different {}{}DOCTYPEs: {}", Name, Split, Diff);
#endif
  };

  switch (LHS.Kind) {
  case DTK_Public:
    if (!CompareInlDTD(LHS[3], RHS[3])) {
      LogDifferences("PUBLIC", 3);
      return false;
    }
    [[fallthrough]];
  case DTK_System:
    if (LHS[2] != RHS[2]) {
      LogDifferences("SYSTEM", 2);
      return false;
    }
    [[fallthrough]];
  case DTK_Inline:
    if (!CompareInlDTD(LHS[1], RHS[1])) {
      LogDifferences("Inline", 1);
      return false;
    }
    [[fallthrough]];
  case DTK_None:
    if (LHS[0] != RHS[0]) {
      LogDifferences("", 0);
      return false;
    }
    return true;
  }

  exi_guardrail("Invalid DTD type!");
}
#endif

//////////////////////////////////////////////////////////////////////////

namespace {

template <bool ReturnMatch>
class XMLMatcher;

template <>
class XMLMatcher<false> {
  static constexpr bool ReturnMatch = false;
  using enum XMLCoderOptions::PreserveCDATAKind;
  static constexpr StrRef knode_unknown = "??";
  static constexpr StrRef knode_null = "NULL"; 

  const XMLNode *In, *Out;
  /// Current traversal depth.
  i32 Depth = 0;
  /// Filter for nodes.
  NodeKindBitset Filter = kBitsetDefault;
  /// Compare with prefixes?
  bool PreservePrefixes = true;
  /// What to expect from CDATA nodes.
  XMLCoderOptions::PreserveCDATAKind PreserveCDATA = CDATA_PRESERVE;
  /// ...
  bool ExificientCompatibility = false;
  /// Output stream for error messages.
  raw_ostream& OS;
  /// List of matches, if applicable.
  SmallVecImpl<MatchResult>* Matches = nullptr;

public:
  XMLMatcher(const XMLDocument* In, const XMLDocument* Out,
             raw_ostream& OS, SmallVecImpl<MatchResult>* Matches = nullptr)
   : In(In), Out(Out), OS(OS), Matches(Matches) {}

  void setupPreserve(const XMLCompareOptions& Opts) {
    if (Opts.Preserve) {
      Filter = MakeBitset(*Opts.Preserve);
      PreservePrefixes = Opts.Preserve->Prefixes;
    }
    PreserveCDATA = Opts.PreserveCDATA;
    ExificientCompatibility = Opts.ExificientCompatibility;
  }
  
  bool run() {
    exi_relassert(In->type() == NodeKind::node_document);
    exi_relassert(Out->type() == NodeKind::node_document);
    
    In = In->first_node();
    Out = Out->first_node();

    if (!In || !Out) {
      if (!In && !Out)
        return true;
      else if EXI_UNLIKELY(!In)
        log() << "In is empty but Out has value?\n";
      else
        log() << "Out is empty\n";
      return false;
    }

    ++Depth;
    
    while (Out->type() != NodeKind::node_document) {
      switch (Out->type()) {
      case NodeKind::node_element:
        if (!compareNames(In, Out))
          return false;
        if (!compareAttrs())
          return false;
        break;
      // TODO: Update these comparisons.
      case NodeKind::node_cdata:
        if EXI_NEVER(PreserveCDATA != CDATA_PRESERVE) {
          log() << "Out contains CDATA but did not preserve it?\n";
          return false;
        }
        if (In->type() != NodeKind::node_cdata) {
          log() << "expected CDATA from In, found data instead\n";
          return false;
        }
        if (!compareText(In, Out))
          return false;
        break;
      case NodeKind::node_data:
        if (PreserveCDATA != CDATA_PRESERVE) {
          if (!coalesceAndCompareText())
            return false;
          break;
        }
        [[fallthrough]];
      case NodeKind::node_comment:
        if (!compareText(In, Out))
          return false;
        break;
      case NodeKind::node_doctype:
        if (!compareDTDs())
          return false;
        break;
      case NodeKind::node_pi:
        if (!comparePIs())
          return false;
        break;
      default:
        break;
      }

      if (!advancePositions())
        return false;
    }

    return true;
  }

private:
  static StrRef NodeTypeName(NodeKind Kind) {
    static constexpr StringLiteral Names[] {
      "SD", "SE",
      "CH", "CH(cdata)",
      "CM", "PI(decl)",
      "DT", "PI"
    };

    const int Off = exi::to_underlying(Kind);
    if (Off < int(NodeKind::node_last))
      return Names[Off];
    return knode_unknown;
  }

  static StrRef Type(const XMLNode* N) {
    return EXI_LIKELY(N) ? NodeTypeName(N->type()) : knode_null;
  }

  static StrRef Type(const XMLAttribute* N) {
    if EXI_UNLIKELY(!N)
      return knode_null;
    else if EXI_UNLIKELY(N->id_kind() == xml::IK_None)
      return knode_unknown;
    else
      return N->is_name() ? "AT" : "NS";
  }

  /// Maps CDATA to data for comparison.
  static NodeKind CmpType(NodeKind Kind) {
    if (Kind == NodeKind::node_cdata)
      return NodeKind::node_data;
    return Kind;
  }
  static NodeKind CmpType(const XMLNode* N) {
    exi_invariant(N != nullptr);
    return CmpType(N->type());
  }

  static bool IsData(const XMLNode* N) {
    if (!N)
      return false;
    return CmpType(N->type()) == NodeKind::node_data;
  }

  /// Depth-first search
  static const XMLNode* Advance(const XMLNode*& N, i32& Depth) {
    exi_invariant(N != nullptr);
    if (auto* Child = N->first_node()) {
      ++Depth;
      return (N = Child);
    }
    else if (auto* Next = N->next_sibling())
      return (N = Next);
    --Depth;
    N = N->parent();
    while (N->type() != NodeKind::node_document) {
      exi_assert(N != nullptr);
      if (auto* Next = N->next_sibling())
        return (N = Next);
      --Depth;
      N = N->parent();
    }
    // End of list.
    return N;
  }

  bool advancePositions() {
    i32 OutDepth = Depth, InDepth = Depth;
    Advance(Out, OutDepth);
    if (!Filter.test(Out->type())) {
      OS << format("[{}] found filtered type in Out: {}\n",
                   OutDepth, Type(Out));
      return false;
    }

    Advance(In, InDepth);
    while (!Filter.test(In->type()))
      Advance(In, InDepth);
    
    exi_assert(OutDepth >= 0, "Negative depth?");
    exi_assert(InDepth  >= 0, "Negative depth?");

    if (CmpType(In) != CmpType(Out)) {
      if (InDepth == OutDepth)
        OS << format("[{}] ", InDepth);
      else
        OS << format("[{}-{}] ", InDepth, OutDepth);
      // TODO: For match, set an error
      OS << format("types do not match: {} / {}\n",
                   Type(In), Type(Out));
      return false;
    }

    if EXI_UNLIKELY(InDepth != OutDepth) {
      OS << format("[{}-{}:{}] depths do not match\n",
                   InDepth, OutDepth, Type(Out));
      return false;
    }

    Depth = InDepth;
    return true;
  }

  using LoadedAttrT = std::pair<StrRef, const XMLAttribute*>;

  static void SortAttrs(SmallVecImpl<LoadedAttrT>& V) {
    std::sort(V.begin(), V.end(),
    [] (const LoadedAttrT& LHS, const LoadedAttrT& RHS) -> bool {
      return LHS.first < RHS.first;
    });
  }

  static void LoadAttrs(SmallVecImpl<LoadedAttrT>& V, const XMLNode* N) {
    const XMLAttribute* A = N->first_attribute();
    while (A) {
      V.emplace_back(A->name(), A);
      A = A->next_attribute();
    }
  }

  /// This is needed because for some reason exificient adds it!
  bool isTopLevel_xmlnsxsi(const XMLAttribute* A) {
    if (Depth > 1)
      return false;
    return A->name() == "xmlns:xsi";
  }

  template <bool Validate = false>
  bool loadAttrs(SmallVecImpl<LoadedAttrT>& V, const XMLNode* N) {
    if (PreservePrefixes) {
      LoadAttrs(V, N);
      return true;
    }

    const XMLAttribute* A = N->first_attribute();
    while (A) {
      if (!A->is_namespace()) {
        if (Validate && !ExificientCompatibility)
          V.emplace_back(A->name(), A);
        else {
          auto [NS, Name] = A->name().split(':');
          V.emplace_back(Name, A);
        }
      } else if constexpr (Validate) {
        if (!ExificientCompatibility && !isTopLevel_xmlnsxsi(A)) {
          log("AT") << format("prefixes not preserved, "
                              "but namespace '{}' found in Out\n",
                              A->name());
          return false;
        }
      }
      A = A->next_attribute();
    }

    return true;
  }

  bool compareAttrs() {
    SmallVec<LoadedAttrT, 4> InA, OutA;
    loadAttrs<false>(InA, In);
    OutA.reserve(InA.size());
    if (!loadAttrs</*Validate=*/true>(OutA, Out))
      return false;

    // TODO: Make sure to add option to remove xsi:nil for exificient
    if (InA.size() != OutA.size()) {
      log() << format("elements have different attribute counts: {} / {}\n",
                      InA.size(), OutA.size());
      return false;
    }

    SortAttrs(InA); SortAttrs(OutA);
    for (auto [LHS, RHS] : exi::zip_equal(InA, OutA)) {
      const XMLAttribute *LHSv = LHS.second, *RHSv = RHS.second;
      if (PreservePrefixes && LHSv->id_kind() != RHSv->id_kind()) {
        log() << format("attribute types do not match: {} / {}\n",
                        Type(LHSv), Type(RHSv));
        return false;
      }
      if (LHS.first != RHS.first) {
        log(LHSv) << format("attribute names do not match: {} / {}\n",
                           LHSv->name(), RHSv->name());
        return false;
      }
      if (LHSv->value() != RHSv->value()) {
        log(LHSv) << format("attribute values do not match: \"{}\" / \"{}\"\n",
                           LHSv->name(), RHSv->name());
        return false;
      }
    }

    return true;
  }

  template <typename NodeT>
  EXI_NO_INLINE bool compareNamesExificientNP(const NodeT* LHS, const NodeT* RHS) {
    auto [LNS, LName] = LHS->name().split_back(':');
    auto [RNS, RName] = RHS->name().split_back(':');

    auto LogName = [this] (StrRef NS, StrRef Name) {
      if (NS.empty())
        OS << Name;
      else
        OS << format("({}:){}", NS, Name);
    };

    if (LName != RName) {
      log(LHS) << "names do not match: ";
      LogName(LNS, LName);
      OS << " / ";
      LogName(RNS, RName);
      OS << '\n';
      return false;
    }

    return true;
  }

  template <typename NodeT>
  bool compareNames(const NodeT* LHS, const NodeT* RHS) {
    if (PreservePrefixes) {
      if (LHS->name() != RHS->name()) {
        log(LHS) << format("names do not match: {} / {}\n",
                           LHS->name(), RHS->name());
        return false;
      }
      return true;
    }

    if (ExificientCompatibility)
      tail_return compareNamesExificientNP(LHS, RHS);

    auto [NS, Name] = LHS->name().split_back(':');
    if (Name != RHS->name()) {
      log(RHS) << "names do not match: ";
      if (NS.empty())
        OS << format("{} / {}\n", Name, RHS->name());
      else
        OS << format("({}:){} / {}\n", NS, Name, RHS->name());
      return false;
    }

    return true;
  }

  bool compareText(StrRef LHS, StrRef RHS) {
    StrRef LHSTok = GetNextToken(LHS);
    StrRef RHSTok = GetNextToken(RHS);

    while (!LHSTok.empty() && !RHSTok.empty()) {
      if (LHSTok != RHSTok) {
        log() << format("data does not match: '{}' / '{}'\n",
                        escape::cstyle(LHSTok),
                        escape::cstyle(RHSTok));
        return false;
      }
      LHSTok = GetNextToken(LHS);
      RHSTok = GetNextToken(RHS);
    }

    if (LHSTok.empty() && RHSTok.empty())
      return true;
    
    if (LHSTok.empty()) {
      log() << format("Out has extra data: '{}'\n",
                      escape::cstyle(RHSTok));
    } else /*RHSTok.empty()*/ {
      log() << format("Out is missing data: '{}'\n",
                      escape::cstyle(LHSTok));
    }
    return false;
  }

  ALWAYS_INLINE bool compareText(const XMLNode* LHS, const XMLNode* RHS) {
    return compareText(LHS->value(), RHS->value());
  }

  bool coalesceAndCompareText() {
    using enum NodeKind;
    exi_invariant(PreserveCDATA != CDATA_PRESERVE);
    exi_invariant(IsData(In), "Expected data or CDATA!");

    if (In->type() == node_data && !IsData(In->next_sibling()))
      // Exit early in the simplest case.
      return compareText(In, Out);
    
    SmallStr<64> Buf;
    raw_svector_ostream OS(Buf);
    const XMLNode* N = In;
    const bool NeedsEscape = (PreserveCDATA == CDATA_ESCAPE);

    do {
      // We know N must be data at the start of every loop.
      this->In = N;
      // If data or CDATA w/ exificient compat enabled.
      if (N->type() == node_data || !NeedsEscape)
        OS << N->value();
      else
        OS << escape::xml(N->value());
      OS << '\n';
      N = N->next_sibling();
    } while (IsData(N));

    // Remove the last newline to avoid doing extra work.
    Buf.pop_back();
    return compareText(Buf.str(), Out->value());
  }

  ExiResult<DoctypeEvent> createDTEvent(const XMLNode* N, StrRef Which) {
    ExiResult<DoctypeEvent> DTD
      = DTDParser::CreateDTEvent(N->value());
    if EXI_UNLIKELY(DTD.is_err()) {
      log() << format("failed to create DOCTYPE for {}: '{}'\n",
                      Which, escape::cstylenq(N->value()));
    }
    return DTD;
  }

  bool compareDTDs() {
    auto InDTD = createDTEvent(In, "In");
    if (InDTD.is_err())
      return false;
    auto OutDTD = createDTEvent(Out, "Out");
    if (OutDTD.is_err())
      return false;
    //return DebugCompareDTDs(*InDTD, *OutDTD);
    if (InDTD->Kind != OutDTD->Kind) {
      log() << format("doctype types did not match: {} / {}\n",
                      get_doctype_name(InDTD->Kind),
                      get_doctype_name(OutDTD->Kind));
      return false;
    } else if (InDTD->name() != OutDTD->name()) {
      log() << format("doctype names did not match: {} / {}\n",
                       InDTD->name(),
                      OutDTD->name());
      return false;
    } else if (InDTD->publicID() != OutDTD->publicID()) {
      log() << format("doctype publicIDs did not match: {} / {}\n",
                       InDTD->publicID(),
                      OutDTD->publicID());
      return false;
    } else if (InDTD->systemID() != OutDTD->systemID()) {
      log() << format("doctype systemIDs did not match: {} / {}\n",
                       InDTD->systemID(),
                      OutDTD->systemID());
      return false;
    }
    return compareText(InDTD->text(), OutDTD->text());
  }

  bool comparePIs() {
    if (In->name() != Out->name()) {
      log() << format("names do not match: '{}' / '{}'\n",
                      In->name(), Out->name());
      return false;
    }
    return this->compareAttrs();
  }

  raw_ostream& log() {
    return OS << format("[{}:{}] ", Depth, Type(Out));
  }
  raw_ostream& log(const XMLNode* N) {
    return OS << format("[{}:{}] ", Depth, Type(N));
  }
  raw_ostream& log(const XMLAttribute* N) {
    return OS << format("[{}:{}] ", Depth, Type(N));
  }
  raw_ostream& log(StrRef S) {
    if EXI_UNLIKELY(S.empty())
      return OS << format("[{}] ", Depth);
    return OS << format("[{}:{}] ", Depth, S);
  }
};

} // namespace `anonymous`

bool exi::matchXML(const XMLDocument* In, const XMLDocument* Out,
                   SmallVecImpl<MatchResult>& Matches,
                   const XMLCompareOptions& Opts) {
  exi_todo("Implement matchXML");
  //raw_ostream& OS = Opts.OS.value_or(nulls());
  //XMLMatcher<true> M(In, Out, Opts.OS.?, &Matches);
}

bool exi::compareXML(const XMLDocument* In, const XMLDocument* Out,
                     const XMLCompareOptions& Opts) {
  raw_ostream& OS = Opts.OS.value_or(nulls());
  XMLMatcher<false> M(In, Out, OS);
  M.setupPreserve(Opts);
  return M.run();
}
