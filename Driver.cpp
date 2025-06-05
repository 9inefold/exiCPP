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
#include <exi/Decode/XMLSerializer.hpp>
#include <exi/Stream/OrderedReader.hpp>

#include <algorithm>
#include <rapidxml.hpp>

#define DEBUG_TYPE "__DRIVER__"
#define TEST_LARGE_EXAMPLES 0

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
  auto S = decode::BuiltinSchema::New(Opts);
  exi_assert(S, "Invalid BuiltinSchema");

  WithColor(outs(), raw_ostream::BRIGHT_BLUE) << Name << ":\n";
  S->dump();
}

static void TestSchemas() {
  ScopedSave S(DebugFlag, LogLevel::INFO);
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
}

//////////////////////////////////////////////////////////////////////////
// Decoding

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

  if (hasDbgLogLevel(INFO))
    dbgs() << '\n';
  return 0;
}

static int Decode(ExiDecoder& Decoder, MemoryBufferRef MB, Serializer* S) {
  LOG_INFO("Decoding header...");
  if (auto E = Decoder.decodeHeader(MB)) {
    Decoder.diagnose(E);
    return 1;
  }

  LOG_INFO("Decoding body...");
  if (auto E = Decoder.decodeBody(S)) {
    Decoder.diagnose(E);
    return 1;
  }

  if (hasDbgLogLevel(INFO))
    dbgs() << '\n';
  return 0;
}

static int Decode(XMLManager* Mgr, StrRef File, ExiOptions& Opts) {
  XMLContainerRef Exi
    = Mgr->getOptXMLRef(File, errs())
      .expect("could not locate file!");
  auto MB = Exi.getBufferRef();

  LOG_INFO("Decoding: \"{}\"", File);
  ExiDecoder Decoder(Opts, errs());
  return Decode(Decoder, MB);
}

//////////////////////////////////////////////////////////////////////////
// Encoding

static int Encode(XMLManager* Mgr, StrRef File, ExiHeader& Opts) {
  XMLDocument& Xml
    = Mgr->getOptXMLDocument(File, errs())
      .expect("could not locate file!");
  

  LOG_INFO("Encoding: \"{}\"", File);
  // ExiDecoder Decoder(Opts, errs());
  // return Decode(Decoder, MB);
  return 0;
}

//////////////////////////////////////////////////////////////////////////
// Implementation

template <int Total>
EXI_NO_INLINE EXI_NODEBUG static void PrintIters(int NIters) {
  float Percent = (float(NIters) / float(Total)) * 100.f;
  outs() << format(" {: >3.0f}% - {} iterations\n", Percent, NIters);
}

template <int Total, int Divisor = 10>
EXI_INLINE static constexpr bool CheckIters(int& NIters) {
  exi_assume(NIters > -1);
  const bool Out = (NIters++ < Total);
  if EXI_UNLIKELY(NIters % (Total / Divisor) == 0)
    PrintIters<Total>(NIters);
  return Out;
}

static int TestSchemalessDecoding(XMLManagerRef SharedMgr);

int main(int Argc, char* Argv[]) {
  using enum raw_ostream::Colors;
  exi::DebugFlag = LogLevel::WARN;
  HandleEscapeCodeSetup();

  outs().enable_colors(true);
  errs().enable_colors(true);
  dbgs().enable_colors(true);

  XMLManagerRef Mgr = make_refcounted<XMLManager>();

#if 0
  if (int Ret = TestSchemalessDecoding(Mgr)) {
    WithColor OS(outs(), BRIGHT_RED);
    OS << "Decoding failed.\n";
    return Ret;
  }
#endif

  root::FullXMLDump(*Mgr, "examples/Namespace.xml");
  {
    using enum exi::PreserveKind;
    const StrRef File = "examples/NamespaceNooptB.exi";

    XMLContainerRef Exi
      = Mgr->getOptXMLRef(File, errs())
        .expect("could not locate file!");
    auto MB = Exi.getBufferRef();

    const auto Preserve = exi::make_preserve_opts(All & ~LexicalValues);
    ExiOptions Opts {
      .Alignment = AlignKind::BytePacked,
      .Preserve = Preserve
    };
    Opts.SchemaID.emplace(nullptr);

    LOG_INFO("Decoding: \"{}\"", File);
    ExiDecoder Decoder(Opts, errs());
    XMLSerializer S;

    if (int Ret = Decode(Decoder, MB, &S)) {
      WithColor OS(outs(), BRIGHT_RED);
      OS << "Decoding failed.\n";
      return Ret;
    }

    root::FullXMLDump(S.document());
  }
  
  WithColor OS(outs(), BRIGHT_GREEN);
  OS << "Decoding successful!\n";
}

#define DECODE_GENERIC(FUNCTION, FILE, ...) do {                              \
  using enum exi::PreserveKind;                                               \
  const StrRef TheFile = CAT2("examples/" FILE, _str);                        \
  const auto TheOpts = exi::make_preserve_opts(__VA_ARGS__);                  \
  if (int Ret = FUNCTION(TheFile, TheOpts))                                   \
    return Ret;                                                               \
} while(0)

#define DECODE_ORD_BITS(FILE, ...)                                            \
  DECODE_GENERIC(DecodePreserveBits, FILE, __VA_ARGS__)
#define DECODE_ORD_BYTES(FILE, ...)                                           \
  DECODE_GENERIC(DecodePreserveBytes, FILE, __VA_ARGS__)

static int TestSchemalessDecoding(XMLManagerRef SharedMgr) {
  ScopedSave FlagSave(exi::DebugFlag);

  auto DecodeFile = [Mgr = SharedMgr.get()]
   (StrRef HiddenFile, ExiOptions Opts) {
    Opts.SchemaID.emplace(nullptr);
    return Decode(Mgr, HiddenFile, Opts);
  };

  auto DecodePreserveBits = [&DecodeFile]
   (StrRef HiddenFile, ExiOptions::PreserveOpts Preserve = {}) {
    return DecodeFile(HiddenFile, {
      .Alignment = AlignKind::BitPacked,
      .Preserve = Preserve
    });
  };

  auto DecodePreserveBytes = [&DecodeFile]
   (StrRef HiddenFile, ExiOptions::PreserveOpts Preserve = {}) {
    return DecodeFile(HiddenFile, {
      .Alignment = AlignKind::BytePacked,
      .Preserve = Preserve
    });
  };

#if !EXI_LOGGING
  constexpr int MaxIters = 250'000; // 1'000'000;
  WithColor(outs(), BRIGHT_WHITE)
    << "Running tests... " << MaxIters << " iterations.\n";
  // Stress testing in release.
  for (int NIters = 0; CheckIters<MaxIters, 5>(NIters);)
#endif // !EXI_LOGGING
  {
    exi::DebugFlag = LogLevel::VERBOSE;
    // SpecExample.xml with default settings and no options.
    // The example data provided by EXI.
    DECODE_ORD_BITS("SpecExample.exi");
    DECODE_ORD_BYTES("SpecExampleB.exi");

    exi::DebugFlag = LogLevel::INFO;
    // Basic.xml with default settings and no options.
    DECODE_ORD_BITS("BasicNoopt.exi");
    DECODE_ORD_BYTES("BasicNooptB.exi");

    // Customers.xml with Preserve.prefixes and no options.
    // Small namespace example.
    DECODE_ORD_BITS("CustomersNoopt.exi",   Prefixes);
    DECODE_ORD_BYTES("CustomersNooptB.exi", Prefixes);
  }

  exi::DebugFlag = LogLevel::INFO;
  // Thai.xml with default settings and no options.
  // Unicode string example.
  DECODE_ORD_BITS("ThaiNoopt.exi");
  DECODE_ORD_BYTES("ThaiNooptB.exi");

  // Namespace.xml with all content and no options.
  // All features (minus lexical values) tested.
  DECODE_ORD_BITS("NamespaceNoopt.exi",   All & ~LexicalValues);
  DECODE_ORD_BYTES("NamespaceNooptB.exi", All & ~LexicalValues);

#if TEST_LARGE_EXAMPLES
  exi::DebugFlag = LogLevel::WARN;
#if !EXI_LOGGING
  constexpr int MaxLargeIters = 100;
  WithColor(outs(), BRIGHT_WHITE)
    << "Running large tests... " << MaxLargeIters << " iterations.\n";
  // Stress testing in release.
  for (int NIters = 0; CheckIters<MaxLargeIters>(NIters);)
#endif // !EXI_LOGGING
  {
    // Orders.xml with Preserve.prefixes and no options.
    // Has a lot of data with minimal distinct keys.
    DECODE_ORD_BITS("Orders.exi", Prefixes);

    // LineItem.xml with Preserve.prefixes and no options.
    // Has a TON of data with minimal distinct keys.
    DECODE_ORD_BITS("LineItem.exi", Prefixes);

    // treebank_e.xml with Preserve.prefixes and no options.
    // Has 100mb of data in XML form, quite a large test.
    DECODE_ORD_BITS("Treebank.exi", Prefixes);
  }
#endif // TEST_LARGE_EXAMPLES

  return 0;
}
