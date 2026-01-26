//===- exi-test-driver.cpp ------------------------------------------===//
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
#include <exi/Encode/BodyEncoder.hpp>
#include <exi/Encode/XMLSerializer.hpp>
#include <rapidxml.hpp>

#define DEBUG_TYPE "__TESTS__"

using namespace exi;
using namespace driver;

static void PrintHelp() {
  outs() << "USAGE: <xml-in> <exi-out> -X<xml-out> "
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
    } else if (Arg.consume_front("-X")) {
      Arg.consume_pinch("\"");
      Out.FileOut.emplace(Arg.str());
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
  
  StrRef InXml = Args[0];
  StrRef OutExi = Args[1];

  if (classifyXMLKind(InXml) != XMLKind::XmlDocument) {
    WithColor(errs(), BRIGHT_RED)
      << "Invalid in-file '" << InXml << "', expected .xml\n\n";
    return 1;
  }

  if (classifyXMLKind(OutExi) != XMLKind::ExiDocument) {
    WithColor(errs(), BRIGHT_RED)
      << "Invalid out-file '" << OutExi << "', expected .exi\n\n";
    return 1;
  }

  auto InData = LoadFile<true>(InXml);
  ExtraOptions Opts {};
  ParseRemainingArgs(Args.drop_front(2), Opts);

  xml::XMLBumpAllocator Alloc;
  XMLDocument In(Alloc);
  
  XMLParseOptions ParseOpts { .MergeData = true };
  if (ParseXMLFromBuf(In, *InData, ParseOpts))
    return 1;
  
  //////////////////////////////////////////////////////////////
  
  SmallStr<0> EncodeBuf;
  ExiHeaderOnly HdrOnlyOpts {
    .HasCookie = false,
    .HasOptions = false
  };
  ExiOptions CoderOpts {
    .Alignment = Opts.Align,
    .Preserve  = Opts.Preserve,
    .SchemaID  = Some(nullptr)
  };
  
  Result EncoderOrErr = ExiEncoder::New(CoderOpts);
  if (!EncoderOrErr) {
    errs() << EncoderOrErr.error() << '\n';
    WithColor(errs(), BRIGHT_RED)
      << "Encoding failed.\n";
    return 2;
  }

  ExiEncoder Encoder = std::move(*EncoderOrErr);
  Encoder.setHeaderOnly(HdrOnlyOpts)
    .expect("Options already compiled??");

  Result Factory = Encoder.setup();
  if (!Factory) {
    errs() << Factory.error() << '\n';
    WithColor(errs(), BRIGHT_RED)
      << "Encoding failed.\n";
    return 2;
  }

  XMLSerializer SS(In);
  SS.PreserveCDATA = Opts.PreserveCDATA;
  if (auto E = Factory->encode(&SS, EncodeBuf)) {
    errs() << E << '\n';
    WithColor(errs(), BRIGHT_RED)
      << "Encoding failed.\n";
    return 2;
  }

  if (auto E = WriteFile(OutExi, EncodeBuf.str())) {
    logAllUnhandledErrors(std::move(E), errs());
    return 2;
  }

  if (!Opts.FileOut)
    return 0;

  //////////////////////////////////////////////////////////////

  ExiDecoder Decoder(CoderOpts);
  if (auto E = Decoder.decodeHeader(EncodeBuf)) {
    WithColor(errs(), BRIGHT_RED)
      << "Error decoding exi header: " << E << "\n\n";
    return 3;
  }

  XMLDocument Out(Alloc);
  XMLDeserializer DS(Out);
  //if (Opts.PreserveCDATA != CDATA_NONE)
  //  DS.PreserveCDATA = CDATA_ESCAPE;
  //else
  //  DS.PreserveCDATA = Opts.PreserveCDATA;
  DS.PreserveCDATA = CDATA_NONE;

  if (auto E = Decoder.decodeBody(&DS)) {
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

  auto Dump = [&Opts] (XMLDocument& D, SmallVecImpl<char>& V) {
    raw_svector_ostream OS(V);
    OS.enable_colors(false);
    XMLDump::raw(D, XMLDumpOptions {
      .OS                   = OS,
      .IndentScale          = 0,
      .Conforming           = false,
      .PrintRawNames        = Opts.Preserve.Prefixes,
      .Preserve             = Opts.Preserve,
      .PreserveDeclaration  = false,
      .PreserveCDATA        = Opts.PreserveCDATA,
      .EmbeddedCDATA        = true
    });
  };

  SmallStr<0> DecodeBuf;
  Dump(Out, DecodeBuf);

  if (auto E = WriteFile(*Opts.FileOut, DecodeBuf.str())) {
    logAllUnhandledErrors(std::move(E), errs());
    return 4;
  }
}
