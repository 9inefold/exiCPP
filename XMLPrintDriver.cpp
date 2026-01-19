//===- XMLCompareDriver.cpp -----------------------------------------===//
//
// Copyright (C) 2025 Ninefold
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

#include <Common/SmallStr.hpp>
#include <Common/StringSwitch.hpp>
#include <Support/Filesystem.hpp>
#include <Support/Logging.hpp>
#include <Support/MemoryBuffer.hpp>
#include <Support/Path.hpp>
#include <Support/Process.hpp>
#include <Support/raw_ostream.hpp>
#include <exi/Basic/XMLDumper.hpp>
#include <exi/Basic/XMLContainer.hpp>
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
  if (!Env.consumeInteger(10, Int))
    return (Int != 0);
  // Parse string.
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

static bool HandleEscapeCodeSetup() {
  using sys::Process;
  if (Process::IsReallyDebugging()) {
    Option<String> NoAnsiEnv
      = Process::GetEnv("EXICPP_NO_ANSI");
    if (CheckEnvTruthiness(NoAnsiEnv, /*EmptyResult=*/false)) {
      LOG_EXTRA("ANSI escape codes disabled.");
      Process::UseANSIEscapeCodes(false);
      return false;
    }
  }

  LOG_EXTRA("ANSI escape codes enabled.");
  Process::UseANSIEscapeCodes(true);
  Process::UseUTF8Codepage(true);
  return true;
}

static Box<MemoryBuffer> LoadFile(const Twine& Path) {
  SmallStr<80> Storage;
  Path.toVector(Storage);
  sys::fs::make_absolute(Storage);

  auto ErrOrBuf = MemoryBuffer::getFile(Storage.str());
  if (!ErrOrBuf) {
    outs() << raw_ostream::BRIGHT_RED
      << "Error opening file: " << ErrOrBuf.getError().message()
      << "\n" << raw_ostream::RESET << "\n";
    exit(1);
  }

  return std::move(*ErrOrBuf);
}

int main(int Argc, char* Argv[]) {
  using enum raw_ostream::Colors;
  exi::DebugFlag = LogLevel::WARN;
  bool HaveEscapeCodes = HandleEscapeCodeSetup();
  outs().enable_colors(HaveEscapeCodes);
  errs().enable_colors(HaveEscapeCodes);
  dbgs().enable_colors(HaveEscapeCodes);
  
  if (Argc < 2) {
    errs() << "USAGE: <file-in>.xml\n";
    return 1;
  }

  xml::XMLBumpAllocator Alloc;
  XMLDocument In(Alloc);
  auto InData = LoadFile(Argv[1]);
  
  if (Error E = exi::parseXMLFromBuffer(In, *InData)) {
    logAllUnhandledErrors(std::move(E), errs());
    return 1;
  }

  XMLDump::full(In, { .PreserveDeclaration = true });
}
