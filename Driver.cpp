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

enum NodeDataKind {
  NDK_None    = 0b000,
  NDK_Nest    = 0b001,
  NDK_Unnest  = 0b010,
};

using EmbeddedNode = PointerIntPair<XMLNode*, 3, NodeDataKind>;

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

int main(int Argc, char* Argv[]) {
  exi::DebugFlag = LogLevel::INFO;
  HandleEscapeCodeSetup();

  outs().enable_colors(true);
  errs().enable_colors(true);
  dbgs().enable_colors(true);

  XMLManagerRef Mgr = make_refcounted<XMLManager>();

  // Basic.xml with default settings and no options.
  StrRef HiddenFile = "examples/BasicNoopt.exi"_str;
  XMLContainerRef Exi
    = Mgr->getOptXMLRef(HiddenFile, errs())
      .expect("could not locate file!");
  
  ExiDecoder Decode(Exi.getBufferRef(), errs());
  if (!Decode.didHeader())
    return 1;
}
