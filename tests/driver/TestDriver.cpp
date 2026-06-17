//===- TestDriver.cpp -----------------------------------------------===//
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

#include "TestDriver.hpp"
#include <Support/InitDriver.hpp>
#include <Support/Logging.hpp>
#include <Support/Signals.hpp>
#include <Support/raw_ostream.hpp>
#include <exi/Basic/XMLDumper.hpp>
#include <exi/Decode/BodyDecoder.hpp>
#include <exi/Decode/XMLDeserializer.hpp>
#include <exi/Encode/BodyEncoder.hpp>
#include <exi/Encode/XMLSerializer.hpp>
#include <rapidxml.hpp>

#define DEBUG_TYPE "__TEST__"

using namespace exi;
using namespace driver;

// TODO: Use VerboseFail?
static bool VerboseFail = false;
//static Option<String> OriginalXML = std::nullopt;

namespace {
enum FailKind : int {
  FKSuccess     = 0, // All good!
  FKEarly       = 1, // Failed during cl parsing, file not found, etc.
  FKInput       = 2, // Failed while parsing input xml/exi
  FKProcessing  = 3, // Failed during conversion
  FKLate        = 4, // Failed after everything else
};
} // namespace `anonymous`

static void PrintHelp() {
  outs() << "USAGE: [e|d] <mangling>[%<extra>] <file-in> <file-out> "
                   "[-C<cdata-mode>] [-E] [-V] [-T]\n";
                   //"[-C<cdata-mode>] [-X<original>] [-E] [-V] [-T]\n";
  outs().flush();
  exit(1);
}

static bool CheckFileTypes(StrRef InFile, StrRef OutFile, const XMLKind InKind) {
  using enum XMLKind;
  exi_assert(mmatch(InKind).is(XmlDocument, ExiDocument));

  const bool IsInXml = (InKind == XmlDocument);
  const auto OutKind = IsInXml ? ExiDocument : XmlDocument;

  if (classifyXMLKind(InFile) != InKind) {
    StrRef Ex = IsInXml ? ".xml" : ".exi";
    WithColor(errs(), raw_ostream::BRIGHT_RED)
      << "Invalid in-file '" << InFile << "', expected " << Ex << "\n\n";
    return false;
  }

  if (classifyXMLKind(OutFile) != OutKind) {
    StrRef Ex = IsInXml ? ".exi" : ".xml";
    WithColor(errs(), raw_ostream::BRIGHT_RED)
      << "Invalid out-file '" << OutFile << "', expected " << Ex << "\n\n";
    return false;
  }

  return true;
}

static void ParseRemainingArgs(ArrayRef<StrRef> Args, ExtraOptions& Out) {
  for (StrRef Arg : Args) {
    auto CodeOrErr = ParseCommonArgs(Arg, Out);
    if (CodeOrErr.is_err()) {
      StrRef ErrCmd = CodeOrErr.error();
      Arg.consume_front(ErrCmd);
      LOG_WARN("Invalid input for '{}': {}", ErrCmd, Arg);
    } else if (Arg.consume_front("-V"))
      VerboseFail = true;
    //else if (Arg.consume_front("-X"))
    //  OriginalXML.emplace(Arg.str());
    else if (Arg.consume_front("-E"))
      Out.EscapeData = true;
    else if (!*CodeOrErr)
      LOG_WARN("Invalid argument '{}'", Arg);
  }
}

//////////////////////////////////////////////////////////////
// Exi

static int exi_main(ArrayRef<StrRef> Args) {
  using enum raw_ostream::Colors;
  ExiOptions Opts { .SchemaID = Some(nullptr) };
  ExtraOptions ExtraOpts;

  if (!ParseMangling(Args[0], Opts, ExtraOpts)) {
    LOG_ERROR("Failed to parse mangled sequence: {}", Args[0]);
    return FKEarly;
  }

  StrRef InFile = Args[1], OutFile = Args[2];
  if (!CheckFileTypes(InFile, OutFile, XMLKind::ExiDocument))
    return FKEarly;

  // Parse everything else
  ParseRemainingArgs(Args.drop_front(3), ExtraOpts);

  // Input exi file
  // TODO: Readd .MergeData?
  auto InData = LoadFile(InFile);
  XMLParseOptions ParseOpts { /*.MergeData = Opts.MergeData*/ };

  if (VerboseFail)
    exi::DebugFlag = LogLevel::VERBOSE;

  // Set up for decoding
  ExiDecoder Decoder(Opts);
  MemoryBufferRef MB = InData->getMemBufferRef();

  if (auto E = Decoder.decodeHeader(MB)) {
    WithColor(errs(), BRIGHT_RED)
      << "Error decoding exi header: " << E << "\n\n";
    return FKInput;
  }

  // Set up output xml
  xml::XMLBumpAllocator Alloc;
  XMLDocument Out(Alloc);

  // Set up deserialization
  XMLDeserializer S(Out);
  S.PreserveCDATA = ExtraOpts.PreserveCDATA;
  //S.SkipEmptyCH = true;

  if (auto E = Decoder.decodeBody(&S)) {
    WithColor(errs(), BRIGHT_RED)
      << "Error decoding exi body: " << E << "\n\n";
    return FKInput;
  }

#if EXI_DEBUG
  auto HOOptsOrErr = Decoder.headerOnly();
  if (HOOptsOrErr.is_err()) {
    WithColor(errs(), BRIGHT_RED)
      << "Decoding failed? "
      << HOOptsOrErr.error() << "\n\n";
    return FKProcessing;
  }
#endif

  SmallStr<0> OutData;
  /*do dumping*/ {
    raw_svector_ostream OS(OutData);
    OS.enable_colors(false);
    XMLDump::raw(Out, XMLDumpOptions {
      .OS                   = OS,
      .IndentScale          = 0,
      .Conforming           = false,
      .PrintRawNames        = Opts.Preserve.Prefixes,
      .Preserve             = Opts.Preserve,
      .PreserveDeclaration  = false,
      .PreserveCDATA        = ExtraOpts.PreserveCDATA,
      .EmbeddedCDATA        = true,
      .EscapeData           = false
    });
  }

  if (auto E = WriteFile(OutFile, OutData.str())) {
    logAllUnhandledErrors(std::move(E), errs());
    return FKLate;
  }

  return FKSuccess;
}

//////////////////////////////////////////////////////////////
// Xml

static int xml_main(ArrayRef<StrRef> Args) {
  using enum raw_ostream::Colors;
  ExiOptions Opts { .SchemaID = Some(nullptr) };
  ExtraOptions ExtraOpts;
  
  if (!ParseMangling(Args[0], Opts, ExtraOpts)) {
    LOG_ERROR("Failed to parse mangled sequence: {}", Args[0]);
    return FKEarly;
  }

  StrRef InFile = Args[1], OutFile = Args[2];
  if (!CheckFileTypes(InFile, OutFile, XMLKind::XmlDocument))
    return FKEarly;

  // Parse everything else
  ParseRemainingArgs(Args.drop_front(3), ExtraOpts);

  // Input xml file
  auto InData = LoadFile(InFile);
  XMLParseOptions ParseOpts { .MergeData = true };

  // Set up for encoding
  xml::XMLBumpAllocator Alloc;
  XMLDocument In(Alloc);
  
  if (ParseXMLFromBuf(In, *InData, ParseOpts))
    return FKInput;
  
  SmallStr<0> OutData;
  ExiHeaderOnly HdrOnlyOpts {
    .HasCookie = false,
    .HasOptions = false
  };

  if (VerboseFail)
    exi::DebugFlag = LogLevel::VERBOSE;
  
  // Try creating encoder
  Result EncoderOrErr = ExiEncoder::New(Opts);
  if (!EncoderOrErr) {
    errs() << EncoderOrErr.error() << '\n';
    WithColor(errs(), BRIGHT_RED)
      << "Encoding failed.\n";
    return FKProcessing;
  }

  ExiEncoder Encoder = std::move(*EncoderOrErr);
  Encoder.setHeaderOnly(HdrOnlyOpts)
    .expect("Options already compiled??");

  Result Factory = Encoder.setup();
  if (!Factory) {
    errs() << Factory.error() << '\n';
    WithColor(errs(), BRIGHT_RED)
      << "Encoding failed.\n";
    return FKProcessing;
  }

  // Encode as xml
  XMLSerializer S(In);
  S.PreserveCDATA = ExtraOpts.PreserveCDATA;
  if (auto E = Factory->encode(&S, OutData)) {
    errs() << E << '\n';
    WithColor(errs(), BRIGHT_RED)
      << "Encoding failed.\n";
    return FKProcessing;
  }

  if (auto E = WriteFile(OutFile, OutData.str())) {
    logAllUnhandledErrors(std::move(E), errs());
    return FKLate;
  }

  return FKSuccess;
}

//////////////////////////////////////////////////////////////

int main(int Argc, char* Argv[]) {
  InitDriver X(Argc, Argv);
  sys::SetGlobalStackTraceOptions({
    .PrintMultiline = true,
    .ColoredOutput = true,
    .TrimFileNames = true,
    .PrintPC = false,
    .PrintModule = false,
    .DemangleFunctionName = true
  });

  SmallVec<StrRef> ArgStorage;
  for (auto* Arg : ArrayRef(Argv + 1, Argc - 1))
    ArgStorage.emplace_back(Arg);
  ArrayRef Args = ArgStorage;

  if (Args.size() < 4)
    PrintHelp();

  if (Args[0] == "d") // encode
    return exi_main(Args.drop_front());
  else if (Args[0] == "e") // decode
    return xml_main(Args.drop_front());
  else if (Args[0] == "x")
    exi_unimplemented("xml -> xml tests are unimplemented");
  else {
    LOG_ERROR("First argument should be 'e' or 'd', got: {}", Args[0]);
    PrintHelp();
  }
}
