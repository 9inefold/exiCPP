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
#include <Support/Filesystem.hpp>
#include <Support/Logging.hpp>
#include <Support/MemoryBuffer.hpp>
#include <Support/Path.hpp>
#include <Support/WithColor.hpp>
#include <Support/raw_ostream.hpp>
#include <exi/Basic/ExiOptions.hpp>
#include <exi/Basic/XMLManager.hpp>
#include <exi/Basic/XMLCompare.hpp>
#include <exi/Basic/XMLContainer.hpp>
#include <rapidxml.hpp>

#define DEBUG_TYPE "__DRIVER__"

using namespace exi;

namespace {
#ifndef NDEBUG
constexpr int kValidate = xml::parse_validate_closing_tags;
#else
constexpr int kValidate = 0;
#endif

constexpr int kDefault = xml::parse_no_entity_translation
                       /*| xml::parse_no_data_nodes*/
                       | kValidate;

constexpr int kImmutable = kDefault
                         | xml::parse_non_destructive;
} // namespace `anonymous`

static void PrintHelp() {
  outs() << "USAGE: <file-in> <file-out> [c|d|l|i]\n";
  exit(1);
}

static Box<WritableMemoryBuffer> LoadFile(const Twine& Path) {
  SmallStr<80> Storage;
  Path.toVector(Storage);
  sys::fs::make_absolute(Storage);

  auto ErrOrBuf = WritableMemoryBuffer::getFileEx(Storage.str());
  if (!ErrOrBuf) {
    outs() << raw_ostream::BRIGHT_RED
      << "Error opening file: " << ErrOrBuf.getError().message()
      << "\n" << raw_ostream::BRIGHT_WHITE << "\n";
    exit(1);
  }

  return std::move(*ErrOrBuf);
}

static void ParsePreserveOpts(ExiOptions::PreserveOpts& Opts, StrRef A) {
  for (char C : A) {
    switch (C) {
    case 'c':
    case 'C':
      Opts.Comments = true;
      break;
    case 'd':
    case 'D':
      Opts.DTDs = true;
      break;
    case 'l':
    case 'L':
      Opts.LexicalValues = true;
      break;
    case 'i':
    case 'I':
      Opts.PIs = true;
      break;
    default:
      LOG_WARN("Unknown character '{}'", C);
    }
  }
}

int main(int Argc, char* Argv[]) {
  using enum raw_ostream::Colors;
  exi::DebugFlag = LogLevel::WARN;
  outs().enable_colors(true);
  errs().enable_colors(true);
  dbgs().enable_colors(true);

  StrRef Location = *Argv;
  --Argc; ++Argv;

  if (Argc < 2)
    PrintHelp();

  auto InData = LoadFile(Argv[0]);
  auto OutData = LoadFile(Argv[1]);

  xml::XMLBumpAllocator Alloc;
  XMLDocument In(Alloc), Out(Alloc);

  ExiOptions::PreserveOpts Preserve { .Prefixes = true };
  if (Argc >= 3)
    ParsePreserveOpts(Preserve, Argv[2]);
  
  In.parse<kImmutable | xml::parse_all>(InData->getBufferStart());
  Out.parse<kImmutable | xml::parse_all>(OutData->getBufferStart());
  
  bool Res = compareXMLWithPreserve(&In, &Out, Preserve);
  if (Res) {
    WithColor(outs(), raw_ostream::BRIGHT_GREEN)
      << format("'{}' is equal to '{}'\n", Argv[0], Argv[1]);
  } else {
    WithColor(outs(), raw_ostream::BRIGHT_RED)
      << format("'{}' is NOT equal to '{}'\n", Argv[0], Argv[1]);
  }
  outs() << raw_ostream::BRIGHT_WHITE << "\n";
}
