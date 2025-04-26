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
#include <Common/APInt.hpp>
#include <Common/EnumArray.hpp>
#include <Common/IntrusiveRefCntPtr.hpp>
#include <Common/MMatch.hpp>
#include <Common/MaybeBox.hpp>
#include <Common/PointerIntPair.hpp>
#include <Common/Poly.hpp>
#include <Common/Result.hpp>
#include <Common/SmallStr.hpp>
#include <Common/StringSwitch.hpp>
#include <Common/Twine.hpp>

#include <Support/Filesystem.hpp>
#include <Support/Logging.hpp>
#include <Support/MemoryBuffer.hpp>
#include <Support/MemoryBufferRef.hpp>
#include <Support/Process.hpp>
#include <Support/ScopedSave.hpp>
#include <Support/Signals.hpp>
#include <Support/raw_ostream.hpp>

#include <exi/Basic/ExiOptions.hpp>
#include <exi/Basic/Runes.hpp>
#include <exi/Basic/StringTables.hpp>
#include <exi/Basic/XMLManager.hpp>
#include <exi/Basic/XMLContainer.hpp>
#include <exi/Decode/BodyDecoder.hpp>
#include <exi/Stream/StreamVariant.hpp>

#include <algorithm>
#include <rapidxml.hpp>

#define DEBUG_TYPE "__DRIVER__"

using namespace exi;

static Option<bool> EnvAsBoolean(StrRef Env) {
  return StringSwitch<Option<bool>>(Env)
    .Cases("TRUE", "YES", "ON", true)
    .Cases("FALSE", "NO", "OFF", false)
    .Default(std::nullopt);
}

static bool CheckEnvTruthiness(StrRef Env, bool EmptyResult = false) {
  if (Env.empty())
    return false;
  
  // Handle integral values.
  i64 Int = 0;
  if (Env.consumeInteger(10, Int)) {
    return (Int != 0);
  }

  return EnvAsBoolean(Env)
    .value_or(EmptyResult);
}

static bool CheckEnvTruthiness(Option<String>& Env,
                               bool EmptyResult = false) {
  if (!Env)
    return EmptyResult;
  return CheckEnvTruthiness(
    *Env, EmptyResult);
}

static void HandleEscapeCodeSetup() {
  using sys::Process;
  if (Process::IsReallyDebugging()) {
    Option<String> NoAnsiEnv
      = Process::GetEnv("EXICPP_NO_ANSI");
    if (!CheckEnvTruthiness(NoAnsiEnv, /*EmptyResult=*/true)) {
      LOG_EXTRA("ANSI escape codes disabled.");
      return;
    }
  }

  LOG_EXTRA("ANSI escape codes enabled.");
  Process::UseANSIEscapeCodes(true);
  Process::UseUTF8Codepage(true);
}

static void RunDumps(XMLManager& Mgr) {
  using namespace root;
  FullXMLDump(Mgr, "examples/022.xml");
  FullXMLDump(Mgr, "examples/044.xml");
  FullXMLDump(Mgr, "examples/079.xml");
  FullXMLDump(Mgr, "examples/085.xml");
  FullXMLDump(Mgr, "examples/103.xml");
  FullXMLDump(Mgr, "examples/116.xml");
  FullXMLDump(Mgr, "examples/Namespace.xml");
  FullXMLDump(Mgr, "examples/SortTest.xml");
  FullXMLDump(Mgr, "examples/Thai.xml");

  // Without prints this runs in 0.2 seconds!
  // FullXMLDump(Mgr, "large-examples/treebank_e.xml");
}

static void TestSchema(StrRef Name, ExiOptions::PreserveOpts Preserve) {
  ExiOptions Opts { .Preserve = Preserve };
  Opts.SchemaID.emplace(nullptr);
  auto S = BuiltinSchema::New(Opts);
  exi_assert(S, "Invalid BuiltinSchema");

  WithColor(outs(), raw_ostream::BRIGHT_BLUE) << Name << ":\n";
  S->dump();
}

static int Decode(ExiDecoder& Decoder, MemoryBufferRef MB) {
  LOG_INFO("Decoding header...");
  if (auto E = Decoder.decodeHeader(MB)) {
    Decoder.diagnose(E);
    return 1;
  }

  LOG_INFO("Decoding body...");
  if (auto E = Decoder.decodeBody()) {
    Decoder.diagnose(E);
    return 1;
  }

  return 0;
}

static int DecodeExample(XMLManagerRef Mgr) {
  // Basic.xml with default settings and no options.
  StrRef HiddenFile = "examples/SpecExample.exi"_str;
  XMLContainerRef Exi
    = Mgr->getOptXMLRef(HiddenFile, errs())
      .expect("could not locate file!");
  auto MB = Exi.getBufferRef();
  
  ExiOptions Opts {};
  Opts.SchemaID.emplace(nullptr);

  LOG_INFO("Decoding: \"{}\"", HiddenFile);
  ExiDecoder Decoder(Opts, errs());
  return Decode(Decoder, MB);
}

static int DecodeBasic(XMLManagerRef Mgr) {
  // Basic.xml with default settings and no options.
  StrRef HiddenFile = "examples/BasicNoopt.exi"_str;
  XMLContainerRef Exi
    = Mgr->getOptXMLRef(HiddenFile, errs())
      .expect("could not locate file!");
  auto MB = Exi.getBufferRef();
  
  ExiOptions Opts {};
  Opts.SchemaID.emplace(nullptr);

  LOG_INFO("Decoding: \"{}\"", HiddenFile);
  ExiDecoder Decoder(Opts, errs());
  return Decode(Decoder, MB);
}

static int DecodeCustomers(XMLManagerRef Mgr) {
  // Customers.xml with Preserve.prefixes and no options.
  StrRef HiddenFile = "examples/BasicNoopt2.exi"_str;
  XMLContainerRef Exi
    = Mgr->getOptXMLRef(HiddenFile, errs())
      .expect("could not locate file!");
  auto MB = Exi.getBufferRef();
  
  ExiOptions Opts {.Preserve { .Prefixes = true }};
  Opts.SchemaID.emplace(nullptr);

  LOG_INFO("Decoding: \"{}\"", HiddenFile);
  ExiDecoder Decoder(Opts, errs());
  return Decode(Decoder, MB);
}

/// From https://www.w3.org/TR/exi-primer/#neitherDecoding
static constexpr u8 Example[] {
  0x42, 0x5B, 0x9B, 0xDD, 0x19, 0x58, 0x9B, 0xDB, 0xDA, 0xD4, 0x15, 0x91, 0x85,
  0xD1, 0x94, 0x30, 0xC8, 0xC0, 0xC0, 0xDC, 0xB4, 0xC0, 0xE4, 0xB4, 0xC4, 0xCB,
  0x20, 0xAD, 0xCD, 0xEE, 0x8C, 0xAA, 0x00, 0x86, 0x19, 0x18, 0x18, 0x1B, 0x96,
  0x98, 0x1B, 0x96, 0x99, 0x19, 0xD4, 0x25, 0x8D, 0x85, 0xD1, 0x95, 0x9D, 0xBD,
  0xC9, 0xE4, 0x15, 0x15, 0x61, 0x26, 0x90, 0x87, 0x37, 0x56, 0x26, 0xA6, 0x56,
  0x37, 0x4C, 0x06, 0x48, 0x2B, 0x13, 0x7B, 0x23, 0xCE, 0x26, 0x88, 0xDE, 0x40,
  0xDC, 0xDE, 0xE8, 0x40, 0xCC, 0xDE, 0xE4, 0xCE, 0xCA, 0xE8, 0x40, 0xD2, 0xE8,
  0x42, 0x64, 0x01, 0x40, 0x00, 0x1E, 0xE6, 0xD0, 0xDE, 0xE0, 0xE0, 0xD2, 0xDC,
  0xCE, 0x40, 0xD8, 0xD2, 0xE6, 0xE8, 0x01, 0xAD, 0xAD, 0x2D, 0x8D, 0x65, 0x84,
  0x0D, 0x0D, 0xED, 0xCC, 0xAF, 0x25
};

static void PrintExample(raw_ostream& OS, int Skip = 0) {
  WithColor C(OS, raw_ostream::BRIGHT_WHITE);
  bool JustPrinted = true;
  int Ix = 0;

  while (Skip >= 64) {
    Skip -= 64;
    Ix += 8;
  }

  OS << format("{:03}: ", Ix);

  const int SkipN = Skip / 8;
  for (int Ix2 = 0; Ix2 < SkipN; ++Ix, ++Ix2) {
    JustPrinted = false;
    OS << "[      ] ";
    if (((Ix + 1) & 0b111) == 0) {
      JustPrinted = true;
      OS << format("\n{:03}: ", (Ix + 1));
    }
    Skip -= 8;
  }

  if (Skip != 0) {
    exi_invariant(Skip < 8);
    JustPrinted = false;

    auto Str = fmt::format("{:08b} ", Example[Ix]);
    auto Slice = StrRef(Str.data(), 8).drop_front(Skip);
    OS << format("{: >8} ", Slice);

    if (((Ix + 1) & 0b111) == 0) {
      JustPrinted = true;
      OS << format("\n{:03}: ", (Ix + 1));
    }

    ++Ix;
    Skip = 0;
  }

  for (; Ix < sizeof(Example); ++Ix) {
    JustPrinted = false;
    OS << format("{:08b} ", Example[Ix]);
    if (((Ix + 1) & 0b111) == 0) {
      JustPrinted = true;
      OS << format("\n{:03}: ", (Ix + 1));
    }
  }

  if (!JustPrinted)
    OS << '\n';
}

int main(int Argc, char* Argv[]) {
  exi::DebugFlag = LogLevel::INFO;
  HandleEscapeCodeSetup();

  outs().enable_colors(true);
  errs().enable_colors(true);
  dbgs().enable_colors(true);

  XMLManagerRef Mgr = make_refcounted<XMLManager>();
#if 0
  if (int Ret = DecodeBasic(Mgr))
    return Ret;
  // if (int Ret = DecodeCustomers(Mgr))
  //   return Ret;
  return 0;
#endif

  exi::DebugFlag = LogLevel::INFO;
#if 0
  TestSchema("Preserve.{CM}", {
    .Comments = true,
  });
  TestSchema("Preserve.{CM, DT}", {
    .Comments = true,
    .DTDs = true,
  });
  TestSchema("Preserve.{PI, NS}", {
    .PIs = true,
    .Prefixes = true,
  });
  TestSchema("Preserve.All", {
    .Comments = true,
    .DTDs = true,
    .PIs = true,
    .Prefixes = true,
  });
#endif

  exi::DebugFlag = LogLevel::VERBOSE;
  // PrintExample(outs(), 251);

  if (int Ret = DecodeExample(Mgr))
    return Ret;

  ExiDecoder Decoder(outs());
  ExiOptions Opts {};
  Opts.SchemaID.emplace(nullptr);
  if (auto E = Decoder.setOptions(Opts)) {
    Decoder.diagnose(E);
    return 1;
  }

  ArrayRef Ex(Example);
  if (auto E = Decoder.setReader(Ex)) {
    Decoder.diagnose(E);
    return 1;
  }

  if (auto E = Decoder.decodeBody()) {
    Decoder.diagnose(E);
    return 1;
  }
}
