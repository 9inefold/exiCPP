//===- Driver.cpp ---------------------------------------------------===//
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

#include "Driver.hpp"
#include <Common/SmallStr.hpp>
#include <Common/IntrusiveRefCntPtr.hpp>
#include <Common/MMatch.hpp>
#include <Common/PointerIntPair.hpp>
#include <Support/Filesystem.hpp>
#include <Support/Logging.hpp>
#include <Support/MemoryBuffer.hpp>
#include <Support/MemoryBufferRef.hpp>
#include <Support/Process.hpp>
#include <Support/ScopedSave.hpp>
#include <Support/Signals.hpp>
#include <Support/raw_ostream.hpp>
#include <exi/Basic/Runes.hpp>
#include <exi/Basic/XMLManager.hpp>
#include <exi/Basic/XMLContainer.hpp>
#include <algorithm>
#include <rapidxml.hpp>

#define DEBUG_TYPE "__DRIVER__"

using namespace exi;

enum NodeDataKind {
  NDK_None    = 0b000,
  NDK_Nest    = 0b001,
  NDK_Unnest  = 0b010,
};

using EmbeddedNode = PointerIntPair<XMLNode*, 3, NodeDataKind>;

static void CheckRuneDecoding(StrRef UTF8, ArrayRef<char32_t> Expect) {
  SmallVec<Rune> Runes;
  if (Expect.back() == 0)
    Expect = Expect.drop_back();
  WithColor(errs()) << "Testing "
    << WithColor::BRIGHT_YELLOW << '"' << UTF8 << '"'
    << WithColor::RESET << ":\n";

  if (!decodeRunes(UTF8, Runes)) {
    WithColor Save(errs(), WithColor::BRIGHT_RED);
    Save << "  error decoding string.\n\n";
    return;
  }

  if (Runes.size() != Expect.size()) {
    WithColor Save(errs(), WithColor::BRIGHT_RED);
    Save << "  size mismatch with expected.\n\n";
    return;
  }

  bool IsSame = true;
  for (i64 I = 0, E = Runes.size(); I != E; ++I) {
    const Rune ExRune = Rune(Expect[I]);
    if (Runes[I] == ExRune)
      continue;
    IsSame = false;
    errs() << "  mismatch at " << I << ": "
      << format("\\{:06x} -> \\{:06x}\n", Runes[I], ExRune);
  }

  if (!IsSame) {
    WithColor Save(errs(), WithColor::BRIGHT_RED);
    Save << "  decoding inconsistent.\n\n";
    return;
  }

  WithColor Save(errs(), WithColor::GREEN);
  Save << "  success!\n\n";
}

int main(int Argc, char* Argv[]) {
  using namespace root;
  // sys::Process::UseANSIEscapeCodes(true);
  // sys::Process::UseUTF8Codepage(true);
  exi::DebugFlag = LogLevel::WARN;

  outs().enable_colors(true);
  dbgs().enable_colors(true);

  XMLManagerRef Mgr = make_refcounted<XMLManager>();
  // FullXMLDump(*Mgr, "examples/022.xml");
  // FullXMLDump(*Mgr, "examples/044.xml");
  // FullXMLDump(*Mgr, "examples/079.xml");
  // FullXMLDump(*Mgr, "examples/085.xml");
  // FullXMLDump(*Mgr, "examples/103.xml");
  // FullXMLDump(*Mgr, "examples/116.xml");
  // FullXMLDump(*Mgr, "examples/Namespace.xml");
  // FullXMLDump(*Mgr, "examples/SortTest.xml");
  // FullXMLDump(*Mgr, "examples/Thai.xml");

  CheckRuneDecoding(u8"Hello world, Καλημέρα κόσμε, コンニチハ",
                     U"Hello world, Καλημέρα κόσμε, コンニチハ");
  CheckRuneDecoding(u8"∮ E⋅da = Q,  n → ∞, ∑ f(i) = ∏ g(i)",
                     U"∮ E⋅da = Q,  n → ∞, ∑ f(i) = ∏ g(i)");
  CheckRuneDecoding(u8"∀x∈ℝ: ⌈x⌉ = −⌊−x⌋, α ∧ ¬β = ¬(¬α ∨ β)",
                     U"∀x∈ℝ: ⌈x⌉ = −⌊−x⌋, α ∧ ¬β = ¬(¬α ∨ β)");
  CheckRuneDecoding(u8"ði ıntəˈnæʃənəl fəˈnɛtık əsoʊsiˈeıʃn",
                     U"ði ıntəˈnæʃənəl fəˈnɛtık əsoʊsiˈeıʃn");
  CheckRuneDecoding(u8"((V⍳V)=⍳⍴V)/V←,V  ⌷←⍳→⍴∆∇⊃‾⍎⍕⌈",
                     U"((V⍳V)=⍳⍴V)/V←,V  ⌷←⍳→⍴∆∇⊃‾⍎⍕⌈");
  CheckRuneDecoding(u8"კონფერენციაზე დასასწრებად, რომელიც გაიმართება",
                     U"კონფერენციაზე დასასწრებად, რომელიც გაიმართება");
  CheckRuneDecoding(u8"๏ แผ่นดินฮั่นเสื่อมโทรมแสนสังเวช  พระปกเกศกองบู๊กู้ขึ้นใหม่",
                     U"๏ แผ่นดินฮั่นเสื่อมโทรมแสนสังเวช  พระปกเกศกองบู๊กู้ขึ้นใหม่");
  CheckRuneDecoding(u8"ሰው እንደቤቱ እንጅ እንደ ጉረቤቱ አይተዳደርም።",
                     U"ሰው እንደቤቱ እንጅ እንደ ጉረቤቱ አይተዳደርም።");
  CheckRuneDecoding(u8"ᚻᛖ ᚳᚹᚫᚦ ᚦᚫᛏ ᚻᛖ ᛒᚢᛞᛖ ᚩᚾ ᚦᚫᛗ ᛚᚪᚾᛞᛖ ᚾᚩᚱᚦᚹᛖᚪᚱᛞᚢᛗ ᚹᛁᚦ ᚦᚪ ᚹᛖᛥᚫ",
                     U"ᚻᛖ ᚳᚹᚫᚦ ᚦᚫᛏ ᚻᛖ ᛒᚢᛞᛖ ᚩᚾ ᚦᚫᛗ ᛚᚪᚾᛞᛖ ᚾᚩᚱᚦᚹᛖᚪᚱᛞᚢᛗ ᚹᛁᚦ ᚦᚪ ᚹᛖᛥᚫ");
                     
  // Without prints this runs in 0.2 seconds!
  // FullXMLDump(*Mgr, "large-examples/treebank_e.xml");
}
