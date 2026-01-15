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
#include <bitset>

#define DEBUG_TYPE "XMLCompare"
#if !EXI_DEV_ONLY
CLANG_IGNORED("-Wunused-private-field")
#endif

using namespace exi;

namespace {

/// Contains data on the kind of nodes allowed.
using NodeKindBitset = std::bitset<usize(NodeKind::node_last)>;
/// Contains: Document, Element, Data, and CDATA.
static constexpr NodeKindBitset kBitsetDefault = 0b0000'1111;

static NodeKindBitset MakeBitset(ExiOptions::PreserveOpts Opts) {
  NodeKindBitset B = kBitsetDefault;
  // Warn on Preserve.Prefixes == false?
  if (Opts.Comments)
    B.set(NodeKind::node_comment, true);
  if (Opts.DTDs)
    B.set(NodeKind::node_doctype, true);
  if (Opts.PIs)
    B.set(NodeKind::node_pi, true);
  return B;
}

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

//////////////////////////////////////////////////////////////////////////

template <bool ReturnMatch>
class XMLMatcher;

template <>
class XMLMatcher<false> {
  static constexpr bool ReturnMatch = false;

  const XMLNode *In, *Out;
  //u32 Depth = 0;
  /// Filter for nodes.
  NodeKindBitset Filter;
  /// List of matches, if applicable.
  SmallVecImpl<MatchResult>* Matches = nullptr;

public:
  XMLMatcher(const XMLDocument* In, const XMLDocument* Out,
             ExiOptions::PreserveOpts Opts,
             SmallVecImpl<MatchResult>* Matches = nullptr)
   : In(In), Out(Out), Filter(MakeBitset(Opts)), Matches(Matches) {}
  
  bool run() {
    exi_assert(In->type() == NodeKind::node_document
            && Out->type() == NodeKind::node_document);
    
    In = In->first_node();
    Out = Out->first_node();
    if (!In || !Out)
      return !In && !Out;
    
    while (Out->type() != NodeKind::node_document) {
      switch (Out->type()) {
      case NodeKind::node_element:
        if (In->name() != Out->name())
          return false;
        if (!this->compareAttrs())
          return false;
        break;
      case NodeKind::node_data:
      case NodeKind::node_cdata:
      case NodeKind::node_comment:
        if (In->value() != Out->value())
          return false;
        break;
      case NodeKind::node_doctype:
        if (!this->compareDTDs())
          return false;
        break;
      case NodeKind::node_pi:
        if (!this->comparePIs())
          return false;
        break;
      default:
        break;
      }

      if (!this->advancePositions())
        return false;
    }

    return true;
  }

private:
  /// Depth-first search
  static const XMLNode* Advance(const XMLNode*& N) {
    exi_invariant(N != nullptr);
    if (auto* Child = N->first_node())
      return (N = Child);
    else if (auto* Next = N->next_sibling())
      return (N = Next);
    N = N->parent();
    while (N->type() != NodeKind::node_document) {
      exi_assert(N != nullptr);
      if (auto* Next = N->next_sibling())
        return (N = Next);
      N = N->parent();
    }
    // End of list.
    return N;
  }

  static void LoadAttrs(SmallVecImpl<const XMLAttribute*>& V, const XMLNode* N) {
    const XMLAttribute* A = N->first_attribute();
    while (A) {
      V.push_back(A);
      A = A->next_attribute();
    }
  }

  static void SortAttrs(SmallVecImpl<const XMLAttribute*>& V) {
    std::sort(V.begin(), V.end(),
    [] (const XMLAttribute* LHS, const XMLAttribute* RHS) -> bool {
      return LHS->name() < RHS->name();
    });
  }

  bool advancePositions() {
    Advance(Out);
    if (!Filter.test(Out->type())) {
      LOG_WARN("Impossible item in Out?");
      return false;
    }

    Advance(In);
    while (!Filter.test(In->type()))
      Advance(In);
    
    // TODO: For match, set an error
    return Out->type() == In->type();
  }

  bool compareAttrs() {
    SmallVec<const XMLAttribute*, 4> InA, OutA;
    LoadAttrs(InA, In);
    OutA.reserve(InA.size());
    LoadAttrs(OutA, Out);

    if (InA.size() != OutA.size())
      return false;

    SortAttrs(InA); SortAttrs(OutA);
    for (auto [LHS, RHS] : exi::zip_equal(InA, OutA)) {
      if (LHS->name() != RHS->name())
        return false;
      if (LHS->value() != RHS->value())
        return false;
    }

    return true;
  }

  bool compareDTDs() {
    auto InDTD = DTDParser::CreateDTEvent(In->value());
    if (InDTD.is_err()) {
      // TODO: Better log error
      LOG_ERROR("Failed to create DOCTYPE for '{}'.", In->value());
      return false;
    }
    auto OutDTD = DTDParser::CreateDTEvent(Out->value());
    if (OutDTD.is_err()) {
      // TODO: Better log error
      LOG_ERROR("Failed to create DOCTYPE for '{}'.", Out->value());
      return false;
    }
    //return *InDTD == *OutDTD;
    return DebugCompareDTDs(*InDTD, *OutDTD);
  }

  bool comparePIs() {
    if (In->name() != Out->name())
      return false;
    return this->compareAttrs();
  }
};

} // namespace `anonymous`

bool exi::matchXMLWithPreserve(const XMLDocument* In, const XMLDocument* Out,
                               ExiOptions::PreserveOpts Opts,
                               SmallVecImpl<MatchResult>& Matches) {
  // ...
  exi_todo("Implement matchXMLWithPreserve");
  //XMLMatcher<true> M(In, Out, Opts, &Matches);
}

bool exi::compareXMLWithPreserve(const XMLDocument* In, const XMLDocument* Out,
                                 ExiOptions::PreserveOpts Opts) {
  XMLMatcher<false> M(In, Out, Opts);
  return M.run();
}
