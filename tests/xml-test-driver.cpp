//===- xml-test-driver.cpp ------------------------------------------===//
//
// Copyright (C) 2026 Ninefold
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

#include "driver.hpp"
#include <Common/SmallStr.hpp>
#include <Support/InitDriver.hpp>
#include <Support/Logging.hpp>
#include <Support/raw_ostream.hpp>
#include <exi/Basic/ExiOptions.hpp>
#include <exi/Basic/XMLContainer.hpp>
#include <exi/Basic/XMLDumper.hpp>
#include <exi/Decode/BodyDecoder.hpp>
#include <exi/Decode/XMLDeserializer.hpp>
#include <rapidxml.hpp>

#define DEBUG_TYPE "__TESTS__"

using namespace exi;
using namespace driver;

static void PrintHelp() {
  outs() << "USAGE: <file-in> <file-out> "
                   "-O[c|d|i] -C<cdata-mode> -A[i|y] [-T]\n";
  exit(1);
}

static void ParseRemainingArgs(ArrayRef<char*> Args, ExtraOptions& Out) {
  for (StrRef Arg : Args) {
    auto CodeOrErr = ParseCommonArgs(Arg, Out);
    if (CodeOrErr.is_err()) {
      StrRef ErrCmd = CodeOrErr.error();
      Arg.consume_front(ErrCmd);
      LOG_WARN("Invalid input for '{}': {}", ErrCmd, Arg);
    } else if (!*CodeOrErr)
      LOG_WARN("Invalid argument '{}'", Arg);
  }
}

int main(int Argc, char* Argv[]) {
  using enum raw_ostream::Colors;
  InitDriver X(Argc, Argv);

  --Argc; ++Argv;
  ArrayRef<char*> Args(Argv, Argc);

  if (Args.size() < 2)
    PrintHelp();
  
  StrRef InFile = Args[0];
  StrRef OutFile = Args[1];

  if (classifyXMLKind(InFile) != XMLKind::XmlDocument) {
    WithColor(errs(), BRIGHT_RED)
      << "Invalid in-file '" << InFile << "', expected .xml\n\n";
    return 1;
  }

  XMLKind OutFileKind = classifyXMLKind(OutFile);
  if (mmatch(OutFileKind).isnt(XMLKind::XmlDocument,
                               XMLKind::ExiDocument)) {
    WithColor(errs(), BRIGHT_RED)
      << "Invalid out-file '" << OutFile << "', expected .xml or .exi\n\n";
    return 1;
  }

  auto InData = LoadFile(InFile);
  auto OutData = LoadFile(OutFile);

  ExtraOptions Opts {};
  ParseRemainingArgs(Args.drop_front(2), Opts);

  xml::XMLBumpAllocator Alloc;
  XMLDocument In(Alloc);
  
  if (ParseXMLFromBuf(In, *InData))
    return 2;
  
  //////////////////////////////////////////////////////////////
  
  XMLDocument Out(Alloc);
  Option<ExiDecoder> EDecoder;
  Option<XMLDeserializer> ExiS;

  if (OutFileKind == XMLKind::XmlDocument) {
    if (ParseXMLFromBuf(Out, *OutData))
      return 3;
  } else {
    MemoryBufferRef MB = OutData->getMemBufferRef();
    Box<ExiOptions> O(new ExiOptions {
      .Alignment = Opts.Align,
      .Preserve  = Opts.Preserve,
      .SchemaID  = Some(nullptr)
    });

    ExiDecoder& Decoder = EDecoder.emplace(std::move(O));
    XMLDeserializer& S = ExiS.emplace(Out);
    S.PreserveCDATA = XMLCoderOptions::CDATA_ESCAPE;

    if (auto E = Decoder.decodeHeader(MB)) {
      WithColor(errs(), BRIGHT_RED)
        << "Error decoding exi header: " << E << "\n\n";
      return 3;
    }

    if (auto E = Decoder.decodeBody(&S)) {
      WithColor(errs(), BRIGHT_RED)
        << "Error decoding exi body: " << E << "\n\n";
      return 3;
    }

#if EXI_DEBUG
    auto HOOptsOrErr = Decoder.headerOnly();
    if (HOOptsOrErr.is_err()) {
      WithColor(errs(), BRIGHT_RED)
        << "Decoding failed? "
        << HOOptsOrErr.error() << "\n\n";
      return 3;
    }
#endif
  }

  //////////////////////////////////////////////////////////////

  auto Dump = [&Opts] (XMLDocument& D, SmallVecImpl<char>& V) {
    raw_svector_ostream OS(V);
    OS.enable_colors(false);
    XMLDump::full(D, XMLDumpOptions {
      .OS                   = OS,
      .IndentScale          = 0,
      .Conforming           = false,
      .PrintRawNames        = Opts.Preserve.Prefixes,
      .Preserve             = Opts.Preserve,
      .PreserveDeclaration  = false,
      .PreserveCDATA        = Opts.PreserveCDATA
    });
  };

  SmallStr<0> InDump, OutDump;
  Dump(In,  InDump);
  Dump(Out, OutDump);

  if (InDump.str() == OutDump.str()) {
    WithColor(outs(), BRIGHT_GREEN)
      << format("'{}' is equal to '{}'\n", Args[0], Args[1]);
    return 0;
  } else {
    WithColor(outs(), BRIGHT_RED)
      << format("'{}' is NOT equal to '{}'\n", Args[0], Args[1]);
    return 4;
  }
}
