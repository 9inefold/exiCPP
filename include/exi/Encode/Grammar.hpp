//===- exi/Encode/Grammar.hpp ---------------------------------------===//
//
// Copyright (C) 2024 Ninefold
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
/// This file defines the base for encoder grammars.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/FoldingSet.hpp>
#include <core/Common/Option.hpp>
#include <core/Common/Result.hpp>
#include <core/Common/SmallVec.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/CompactID.hpp>
#include <exi/Basic/EventCodes.hpp>
#include <exi/Encode/BodyEncoderAlloc.hpp>
#include <exi/Encode/GrammarNodes.hpp>
#include <exi/Stream/OrderedWriter.hpp>

#define DEBUG_TYPE "Grammar"

namespace exi {
class BodyEncoder;

namespace encode {

/// The base for all grammars.
class alignas(8) Grammar {
  // Grammar impl...
};

/// The grammars for `BuiltinSchema`.
/// Grammars represent codes attached to SE events.
class BuiltinGrammar final : public Grammar {
  /// The LocalName bound to this grammar.
  LocalNameInfo* Name = nullptr;
  u32 StartTagLog = 0, ElementLog = 1;
  FoldingSet<gnode::BaseNode> StartTag; // +1
  FoldingSet<gnode::BaseNode> Element;  // +2
  FirstLevelProd StartTagCH = kInvalidFLProd;
  FirstLevelProd ElementCH = kInvalidFLProd;
  FirstLevelProd StartTagEE = kInvalidFLProd;

  /// Writes a first-level SE term to the stream.
  /// @returns nonnull if second-level encoding is needed.
  template <class NodeT, bool IsStart, is_ordwriter_stream StrmT>
  void* setSEOrATTerm(StrmT* Writer, LocalNameInfo* LN) {
    auto [SEOrAT, IP] = profileTerm<NodeT, IsStart>(LN);
    if (SEOrAT != nullptr) {
      writeFoundCode<IsStart>(Writer, SEOrAT->code());
      return nullptr;
    }
    exi_invariant(IP != nullptr, "Invalid SE/AT insertion point!");
    writeFallbackCode<IsStart>(Writer);
    return IP;
  }

  template <class NodeT, bool IsStart, class Encoder>
  void addSEOrATTerm(Encoder* BE, LocalNameInfo* LN, void* IP) {
    exi_invariant(IP != nullptr, "Invalid SE/AT insertion point!");
    auto& Elts = getElts<IsStart>();
    auto Code = IntCast<u32>(Elts.size());
    auto* SEOrAT = new (*BE) NodeT(Code, LN);
    Elts.InsertNode(SEOrAT, IP);
    this->setLog(IsStart);
  }

  template <class NodeT, bool IsStart, class Encoder>
  void addNewSEOrATTerm(Encoder* BE, LocalNameInfo* LN) {
    auto& Elts = getElts<IsStart>();
    auto Code = IntCast<u32>(Elts.size());
    auto* SEOrAT = new (*BE) NodeT(Code, LN);
    Elts.InsertNode(SEOrAT);
    //auto* N = Elts.GetOrInsertNode(SEOrAT);
    //if (hasDbgLogLevel(WARN) && SEOrAT != N)
    //  BuiltinGrammar::LogDiscarded(LN);
    this->setLog(IsStart);
  }

public:
  explicit BuiltinGrammar(LocalNameInfo* LN) : Name(LN) {
    exi_guard_invariant(LN != nullptr,
      "Grammars cannot have empty names!");
  }

  /// Writes a first-level SE term to the stream.
  /// @returns nonnull if second-level encoding is needed.
  template <bool IsStart, is_ordwriter_stream StrmT>
  void* setSETerm(StrmT* Writer, LocalNameInfo* LN) {
    tail_return this->setSEOrATTerm<gnode::SEQNameNode, IsStart>(Writer, LN);
  }
  /// Writes a first-level AT term to the stream.
  /// @returns nonnull if second-level encoding is needed.
  template <is_ordwriter_stream StrmT>
  void* setATTerm(StrmT* Writer, LocalNameInfo* LN) {
    // AT terms are always StartTagContent.
    tail_return this->setSEOrATTerm<gnode::ATQNameNode, true>(Writer, LN);
  }

  /// Writes a first-level EE term to the stream. Only needs to be checked
  /// dynamically when in `StartTagContent`, as `ElementContent` has a
  /// first-level EE code to start with.
  /// @returns `true` if second-level encoding is needed.
  template <bool IsStart, is_ordwriter_stream StrmT>
  bool setEETerm(StrmT* Writer) {
    if constexpr (IsStart) {
      if (StartTagEE != kInvalidFLProd) {
        writeFoundCode<true>(Writer, StartTagEE);
        return false;
      }
      writeFallbackCode<true>(Writer);
      return true;
    }
    u64 Code = Element.size();
    LOG_EXTRA("Code[0]:   @{}:{}", ElementLog, Code);
    Writer->writeBits64(Code, ElementLog);
    return false;
  }

  /// Writes a first-level CH term to the stream.
  /// @returns `true` if second-level encoding is needed.
  template <bool IsStart, is_ordwriter_stream StrmT>
  bool setCHTerm(StrmT* Writer) {
    const FirstLevelProd CH = getCH<IsStart>();
    if (CH != kInvalidFLProd) {
      writeFoundCode<IsStart>(Writer, CH);
      return false;
    }
    writeFallbackCode<IsStart>(Writer);
    return true;
  }

  template <bool IsStart, class Encoder>
  void addSETerm(Encoder* BE, LocalNameInfo* LN, void* IP) {
    tail_return this->addSEOrATTerm<gnode::SEQNameNode, IsStart>(BE, LN, IP);
  }
  template <class Encoder>
  void addATTerm(Encoder* BE, LocalNameInfo* LN, void* IP) {
    // AT terms are always StartTagContent.
    tail_return this->addSEOrATTerm<gnode::ATQNameNode, true>(BE, LN, IP);
  }
  /// Adds an SE code that can't have been seen.
  template <bool IsStart, class Encoder>
  void addNewSETerm(Encoder* BE, LocalNameInfo* LN) {
    tail_return this->addNewSEOrATTerm<gnode::SEQNameNode, IsStart>(BE, LN);
  }
  /// Adds an AT code that can't have been seen.
  template <class Encoder>
  void addNewATTerm(Encoder* BE, LocalNameInfo* LN) {
    tail_return this->addNewSEOrATTerm<gnode::ATQNameNode, true>(BE, LN);
  }

  template <bool IsStart>
  inline void addEETerm(gnode::EENode* EE) {
    if constexpr (IsStart) {
      exi_invariant(StartTagEE == kInvalidFLProd,
                    "EE Grammar already active!");
      StartTagEE = IntCast<FirstLevelProd>(StartTag.size());
      StartTag.InsertNode(EE);
      this->setLog(IsStart);
    }
  }
  template <bool IsStart>
  void addCHTerm(gnode::CHNode* Node) {
    exi_invariant(getCH<IsStart>() == kInvalidFLProd,
                  "CH Grammar already active!");
    auto& Elts = getElts<IsStart>();
    const auto CH = IntCast<FirstLevelProd>(Elts.size());
    this->setCH<IsStart>(CH);
    Elts.InsertNode(Node);
    this->setLog(IsStart);
  }

  template <bool IsStart, is_ordwriter_stream StrmT>
  void writeFallbackCode(StrmT* Writer) {
    const u64 Size = getElts<IsStart>().size();
    u64 Code = IsStart ? Size : Size + 1;
    LOG_EXTRA("Code[0]:   @{}:{}", getLog<IsStart>(), Code);
    Writer->writeBits64(Code, getLog<IsStart>());
  }

  LocalNameInfo* getName() const { return Name; }
  void dump(BodyEncoder* E) const;

private:
  template <class NodeT, bool IsStart>
  std::pair<NodeT*, void*> profileTerm(auto&&...Args) {
    FoldingSetNodeID ID;
    NodeT::ProfileImpl(ID, Args...);
    void* InsertPos;
    gnode::BaseNode* Val = getElts<IsStart>()
        .FindNodeOrInsertPos(ID, InsertPos);
    return {cast_if_present<NodeT>(Val), InsertPos};
  }

  template <bool IsStart, is_ordwriter_stream StrmT>
  void writeFoundCode(StrmT* Writer, u64 OGCode) {
    const u64 Size = getElts<IsStart>().size();
    exi_invariant(OGCode < Size && OGCode != kInvalidFLProd);
    u64 Code = (Size - 1) - OGCode;
    LOG_EXTRA("Code[0]:   @{}:{}", getLog<IsStart>(), Code);
    Writer->writeBits64(Code, getLog<IsStart>());
  }

  template <bool IsStart> inline FirstLevelProd getCH() {
    return IsStart ? StartTagCH : ElementCH;
  }
  template <bool IsStart> ALWAYS_INLINE void setCH(FirstLevelProd CH) {
    if constexpr (IsStart)
      StartTagCH = CH;
    else
      ElementCH  = CH;
  }

  /// Returns a precalculated log for StartTag or Element.
  template <bool IsStart> u32 getLog() const {
    return IsStart ? StartTagLog : ElementLog;
  }

  void setLog(bool IsStart) {
    if (IsStart) {
      const u32 Log = StartTag.size() + 1;
      StartTagLog = ID_Log2(Log);
    } else {
      const u32 Log = Element.size() + 2;
      ElementLog = ID_Log2(Log);
    }
  }

  /// Returns the backing for StartTag or Element.
  template <bool IsStart> 
  FoldingSet<gnode::BaseNode>& getElts() {
    if constexpr (IsStart)
      return StartTag;
    else
      return Element;
  }

  static void LogDiscarded(LocalNameInfo* LN);
};

} // namespace encode
} // namespace exi

#undef DEBUG_TYPE
