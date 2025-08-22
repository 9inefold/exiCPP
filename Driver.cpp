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
#include <Support/Path.hpp>
#include <Support/Process.hpp>
#include <Support/ScopedSave.hpp>
#include <Support/Signals.hpp>
#include <Support/raw_ostream.hpp>

#include <exi/Basic/ExiOptions.hpp>
#include <exi/Basic/Runes.hpp>
#include <exi/Basic/StringTables.hpp>
#include <exi/Basic/XMLManager.hpp>
#include <exi/Basic/XMLContainer.hpp>
#include <exi/Stream/OrderedReader.hpp>
#include <exi/Stream/OrderedWriter.hpp>

#include <exi/Encode/BodyEncoder.hpp>
#include <exi/Encode/NamespaceContextStack.hpp>
#include <exi/Encode/XMLSerializer.hpp>

#include <exi/Decode/BodyDecoder.hpp>
#include <exi/Decode/XMLDeserializer.hpp>

#include <algorithm>
#include <rapidxml.hpp>

#define DEBUG_TYPE "__DRIVER__"
#define TEST_LARGE_EXAMPLES 1
#define STRESS_TEST_DECODING 0

using namespace exi;

static constinit Option<String> EXIFICIENT_DIR = std::nullopt;

static void AddExificientCmdOpts(raw_ostream& OS, const ExiOptions& Opts) {
  if (!Opts.SchemaID || !*Opts.SchemaID)
    OS << "-noSchema ";
  else
    OS << "-schema " << *Opts.SchemaID->get() << ' ';
  
  if (Opts.Strict)
    OS << "-strict ";
  if (Opts.Preserve.Prefixes)
    OS << "-preservePrefixes ";
  if (Opts.Preserve.Comments)
    OS << "-preserveComments ";
  if (Opts.Preserve.PIs)
    OS << "-preservePIs ";
  if (Opts.Preserve.DTDs)
    OS << "-preserveDTDs ";
  if (Opts.Preserve.LexicalValues)
    OS << "-preserveLexicalValues ";
  
  if (Opts.Alignment != AlignKind::BitPacked) {
    if (Opts.Alignment == AlignKind::BytePacked)
      OS << "-bytePacked ";
    else if (!Opts.Compression)
      OS << "-preCompression ";
    else
      OS << "-compression ";
  }
}

ALWAYS_INLINE static constexpr 
 void SetLogLevel([[maybe_unused]] LogLevelType NewLevel) {
#if EXI_LOGGING
  exi::DebugFlag = NewLevel;
#endif
}

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

static void HandleDebugSetup() {
#if EXI_DEBUG
  using sys::Process;
  Option<String> TrappingEnv
    = Process::GetEnv("EXICPP_TRAP_ERRORS");
  if (CheckEnvTruthiness(TrappingEnv)) {
    exi::IsDebuggingFlag = true;
    LOG_EXTRA("Debugging enabled.");
  } else
    LOG_EXTRA("Debugging disabled.");
#endif
}

static void RunDumps(XMLManager& Mgr, bool Conforming = false) {
  using namespace root;
  DumpOptions Opts {.Conforming = Conforming};
  FullXMLDump(Mgr, "examples/022.xml", Opts);
  FullXMLDump(Mgr, "examples/044.xml", Opts);
  FullXMLDump(Mgr, "examples/079.xml", Opts);
  FullXMLDump(Mgr, "examples/085.xml", Opts);
  FullXMLDump(Mgr, "examples/103.xml", Opts);
  FullXMLDump(Mgr, "examples/116.xml", Opts);
  FullXMLDump(Mgr, "examples/Namespace.xml", Opts);
  FullXMLDump(Mgr, "examples/SortTest.xml", Opts);
  FullXMLDump(Mgr, "examples/Thai.xml", Opts);

  // Without prints this runs in 0.2 seconds!
  // FullXMLDump(Mgr, "large-examples/treebank_e.xml", Opts);
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
    errs() << E << '\n';
    return 1;
  }

  LOG_INFO("Decoding body...");
  if (auto E = Decoder.decodeBody()) {
    errs() << E << '\n';
    return 1;
  }

  INFO_ONLY(dbgs() << '\n');
  return 0;
}

static int Decode(ExiDecoder& Decoder, MemoryBufferRef MB, Deserializer* S) {
  LOG_INFO("Decoding header...");
  if (auto E = Decoder.decodeHeader(MB)) {
    errs() << E << '\n';
    return 1;
  }

  LOG_INFO("Decoding body...");
  if (auto E = Decoder.decodeBody(S)) {
    errs() << E << '\n';
    return 1;
  }

  INFO_ONLY(dbgs() << '\n');
  return 0;
}

static int Decode(XMLManager* Mgr, StrRef File, ExiOptions& Opts) {
  XMLContainerRef Exi
    = Mgr->getOptXMLRef(File, errs())
      .expect("could not locate file!");
  auto MB = Exi.getBufferRef();

  LOG_INFO("Decoding: \"{}\"", File);
  ExiDecoder Decoder(Opts);
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

//////////////////////////////////////////////////////////////////////////
// Diff

static int CalcSpacesSize(StrRef LHS, StrRef RHS) {
  usize Size = std::min(LHS.size(), RHS.size());
  return Log2_64_Ceil(Size) / 4 + 1;
}

/// Prints a pretty byte diff
static void ByteDiffViewer(StrRef Original, StrRef Encoded,
                           usize BreakOn = 4, bool Hex = false,
                           bool LabelX = false, usize SkipY = 0) {
  using enum raw_ostream::Colors;
  static constexpr StrRef VSplit = " \xE2\x94\x82 "_str;
  const usize BreakOnX = (BreakOn > 0) ? BreakOn : 1;
  usize Width = Hex ? 2 : 8;
  usize BlockSize = (BreakOnX * (Width + 1)) - 1;
  const int Padding = CalcSpacesSize(Original, Encoded);
  usize Count = SkipY, Ix = 0;

  if (SkipY > 0) {
    if (SkipY > (Original.size() / BreakOnX) + 1) {
      LOG_WARN("SkipY of {} is larger than the buffer!", SkipY);
      return;
    }
    Original = Original.drop_front(SkipY * BreakOnX);
    Encoded = Encoded.drop_front(SkipY * BreakOnX);
  }

  SmallStr<0> LHS, RHS;
  raw_svector_ostream LOS(LHS), ROS(RHS);
  LOS.enable_colors(true); ROS.enable_colors(true);
  auto WriteByte = [Hex](raw_ostream& OS, char Byte) {
    if (!Hex)
      OS << format("{:08b} ", Byte);
    else
      OS << format("{:02x} ", Byte);
  };

  WithColor OS(errs(), BRIGHT_WHITE);
  auto DumpData = [&, BreakOnX, Padding](){
    OS << format("0x{:0{}x}", (Count * BreakOnX), Padding)
       << VSplit
       << LHS << BRIGHT_WHITE << VSplit
       << RHS << BRIGHT_WHITE << VSplit << '\n';
    LHS.clear(); RHS.clear();
    ++Count; Ix = 0;
  };

  SmallStr<64> CenterString;
  CenterString.reserve((BlockSize + 1) * 3);
  for (usize N = 0; N < BreakOnX; ++N)
    CenterString.append(Hex ? u8"───"_str : u8"─────────"_str);
  CenterString.pop_back_n(3);

  auto PrintLine = [&] (StrRef LS, StrRef C, StrRef RS) {
    OS << indent(2 + Padding)
       << LS << CenterString
       << C  << CenterString
       << RS << '\n';
  };
  auto PrintEmptyLine = [&] (StrRef S = "") {
    exi_invariant(S.size() <= 5, "This may break formatting!");
    OS << left_justify(S, 2 + Padding) << VSplit
       << indent(BlockSize) << VSplit
       << indent(BlockSize) << VSplit << '\n';
  };

  /*Print Top*/ {
    PrintLine(u8" ┌─", u8"─┬─", u8"─┐ ");
    OS << indent(2 + Padding) << VSplit
      << center_justify("Original", BlockSize) << VSplit
      << center_justify("Encoded", BlockSize) << VSplit << '\n';
    PrintLine(u8" ├─", u8"─┼─", u8"─┤ ");
  } if (LabelX) {
    OS << indent(2 + Padding);
    for (int Groups = 0; Groups < 2; ++Groups) {
      OS << VSplit << left_justify("00", Width);
      for (usize I = 1; I < BreakOn; ++I) {
        auto N = fmt::format("{:02x}", I);
        OS << ' ' << left_justify(N, Width);
      }
    }
    OS << VSplit << '\n';
    // Empty line
    PrintEmptyLine(SkipY > 0 ? "..." : "");
  }

  for (auto [O, E] : exi::zip(Original, Encoded)) {
    auto Color = (O == E) ? BRIGHT_GREEN : BRIGHT_RED;
    WriteByte(LOS << Color, O);
    WriteByte(ROS << Color, E);
    if (++Ix >= BreakOnX) {
      LHS.pop_back(); RHS.pop_back();
      DumpData();
      OS << BRIGHT_WHITE;
    }
  }

  if (!LHS.empty()) {
    usize Remaining = BreakOnX - Ix;
    usize Spaces = (Remaining * (Width + 1)) - 1;
    LHS.append(Spaces, ' ');
    RHS.append(Spaces, ' ');
    DumpData();
  }

  /*Print Bottom*/ {
    PrintEmptyLine();
    PrintLine(u8" └─", u8"─┴─", u8"─┘ ");
  }
}

static void PadByteDiffViewer(StrRef Original, StrRef Encoded,
                              usize BreakOn = 4, bool Hex = false,
                              bool LabelX = false, usize SkipY = 0) {
  if (Original.size() > Encoded.size()) {
    std::string E(Encoded.data(), Encoded.size());
    E.resize(Original.size(), '\0');
    StrRef EData(E.data(), E.size());
    return ByteDiffViewer(Original, EData, BreakOn, Hex, LabelX, SkipY);
  } else if (Original.size() < Encoded.size()) {
    std::string O(Original.data(), Original.size());
    O.resize(Encoded.size(), '\0');
    StrRef OData(O.data(), O.size());
    return ByteDiffViewer(OData, Encoded, BreakOn, Hex, LabelX, SkipY);
  }
  return ByteDiffViewer(Original, Encoded, BreakOn, Hex, LabelX, SkipY);
}

static void PrintByteCompareStrings(bool Val) {
  if (Val) {
    WithColor(errs(), raw_ostream::BRIGHT_GREEN)
      << "Encoded files are the same!\n";
  } else {
    WithColor(errs(), raw_ostream::BRIGHT_RED)
      << "Encoded files are NOT the same.\n";
  }
}

static void ByteCompareStrings(StrRef Original, StrRef Encoded) {
  auto AllZeros = [] (StrRef S) -> bool {
    for (char C : S)
      if (C != 0)
        return false;
    return true;
  };
  usize OSize = Original.size(), ESize = Encoded.size();
  if (OSize > ESize) {
    bool SameFront = (Original.take_front(ESize) == Encoded);
    PrintByteCompareStrings(SameFront
      && AllZeros(Original.take_back(OSize - ESize)));
  } else if (OSize < ESize) {
    bool SameFront = (Original == Encoded.take_front(OSize));
    PrintByteCompareStrings(SameFront
      && AllZeros(Encoded.take_back(ESize - OSize)));
  } else {
    PrintByteCompareStrings(Original == Encoded);
  }
}

//////////////////////////////////////////////////////////////////////////
// Encode/Decode

namespace {
class ECDCTestRunner {
  XMLManagerRef Mgr;
  StrRef ExamplePath = "examples";
  mutable ExiOptions Opts;
  Vec<String>* Cmds = nullptr;

public:
  ECDCTestRunner(XMLManagerRef Mgr, StrRef Folder,
                 AlignKind K, ExiOptions::PreserveOpts P,
                 Vec<String>* Cmds = nullptr)
   : Mgr(std::move(Mgr)), Opts{
      .Alignment = K, .Preserve = P,
      .SchemaID = Some(nullptr)},
     Cmds(Cmds) {
  }

  ECDCTestRunner(XMLManagerRef Mgr,
                 AlignKind K, ExiOptions::PreserveOpts P,
                 Vec<String>* Cmds = nullptr)
   : ECDCTestRunner(std::move(Mgr), "examples", K, P, Cmds) {
  }

  /// Runs Decoder -> Encoder -> Decoder.
  int runWithRet(StrRef ExiFile, StrRef XmlFile,
                 bool Diff = true, bool Dump = false, usize Skip = 0) const;
  /// Invokes run, exits on failure.
  void run(StrRef ExiFile, StrRef XmlFile,
           bool Diff = true, bool Dump = false, usize Skip = 0) const;
  /// Invokes run without exi, exits on failure.
  void runXml(StrRef XmlFile, bool Diff = true,
              bool Dump = false, usize Skip = 0) const {
    this->run(""_str, XmlFile, Diff, Dump, Skip);
  }

  ExiOptions* operator->() { return &Opts; }
  const ExiOptions* operator->() const { return &Opts; }
};
} // namespace `anonymous`

static Expected<std::string> GetAbsoluteFilename(StrRef File) {
  std::string FileName = File.str();
  if (!sys::path::is_absolute(File)) {
    SmallStr<256> Path(File);
    if (auto EC = sys::fs::make_absolute(Path))
      return errorCodeToError(EC);
    FileName = static_cast<std::string>(Path);
  }
  StrRef Parent = sys::path::parent_path(FileName);
  if (auto EC = sys::fs::create_directories(Parent))
    return errorCodeToError(EC);
  return FileName;
}

/// Writes contents to a file.
static Error WriteFile(StrRef File, StrRef Contents) {
  Expected<std::string> ErrOrFileName
    = GetAbsoluteFilename(File);
  if (!ErrOrFileName)
    return ErrOrFileName.takeError();
  std::string FileName = std::move(ErrOrFileName.get());
  
  std::error_code EC;
  raw_fd_ostream OS(FileName, EC,
    sys::fs::CD_CreateAlways,
    sys::fs::FA_Write, sys::fs::OF_None);
  if (EC)
    return errorCodeToError(EC); 
  OS.write(Contents.data(), Contents.size());
  return Error::success();
}

static Error WriteCmdsToFile(StrRef File, const Vec<String>& Contents) {
  Expected<std::string> ErrOrFileName
    = GetAbsoluteFilename(File);
  if (!ErrOrFileName)
    return ErrOrFileName.takeError();
  std::string FileName = std::move(ErrOrFileName.get());

  std::error_code EC;
  raw_fd_ostream OS(FileName, EC,
    sys::fs::CD_CreateAlways,
    sys::fs::FA_Write, sys::fs::OF_None);
  if (EC)
    return errorCodeToError(EC);
#if EXI_ON_WIN32
  OS << "@echo off\n\n";
#endif
  for (const String& Line : Contents)
    OS.write(Line.data(), Line.size()) << '\n';
  OS.flush();
  return Error::success();
}

static void WriteExificientCmds(const Vec<String>& Contents) {
  constexpr StrRef OutputFile = 
#if EXI_ON_WIN32
    "examples/out/ExificientCmds.bat";
#else
    "examples/out/ExificientCmds.sh";
#endif
  Error E = WriteCmdsToFile(OutputFile, Contents);
  logAllUnhandledErrors(std::move(E), errs(), "Errors creating ExificientCmds");
}

int ECDCTestRunner::runWithRet(StrRef ExiFile, StrRef XmlFile,
                               bool Diff, bool Dump, usize Skip) const {
  using enum raw_ostream::Colors;
  const auto CoderLogLevel = exi::DebugFlag;
  root::DumpOptions DO {.Conforming = true};
#if EXI_LOGGING
  ScopedSave LogLevelRestore(exi::DebugFlag, LogLevel::WARN);
#endif

  auto CreateOutPath = [&, this] (SmallVecImpl<char>& Path) -> StrRef {
    if (!ExiFile.empty()) {
      sys::path::append(Path, ExamplePath, "out", ExiFile);
      return StrRef(Path.begin(), Path.size());
    }

    sys::path::append(Path, ExamplePath, "out", sys::path::stem(XmlFile));
    Path.append({'N','o','o','p','t'});
    if (this->Opts.Alignment == AlignKind::BytePacked)
      Path.push_back('B');
    sys::path::replace_extension(Path, ".exi");
    return StrRef(Path.begin(), Path.size());
  };

  SmallStr<80> FilenameStore;
  StrRef OutExiFile = CreateOutPath(FilenameStore);

  MemoryBufferRef DecodeBuf;
  SmallStr<0> EncodeBuf;

  auto PrintHexDiff = [Skip] (StrRef Original, StrRef Encoded) {
    PadByteDiffViewer(Original, Encoded, /*Width=*/16, /*Hex=*/true,
                      /*LabelX=*/true, /*SkipY=*/Skip);
  };
  auto PrintDiffOrCmp = [&, Diff] (StrRef Original, StrRef Encoded) {
    if (Diff)
      PrintHexDiff(Original, Encoded);
    else
      ByteCompareStrings(Original, Encoded);
  };

  Option<ExiDecoder> EDecoder;
  Option<XMLDeserializer> ExiS;

  bool HasCookie = false;
  const bool HasOptions = false;

  if (!ExiFile.empty()) {
    SmallStr<80> File;
    sys::path::append(File, ExamplePath, ExiFile);
    WithColor(errs(), BRIGHT_CYAN)
      << format("Decoding: \"{}\"", File.str()) << '\n';

    XMLContainerRef Exi
      = Mgr->getOptXMLRef(File.str(), errs())
        .expect("could not locate file!");
    auto MB = Exi.getBufferRef();
    DecodeBuf = MB;

    SetLogLevel(LogLevel::WARN);
    ExiDecoder& Decoder = EDecoder.emplace(Opts);
    XMLDeserializer& S = ExiS.emplace();

    SetLogLevel(CoderLogLevel);
    if (int Ret = Decode(Decoder, MB, &S)) {
      WithColor(errs(), BRIGHT_RED)
        << "Decoding failed.\n";
      return Ret;
    }

    HasCookie = Decoder.hasCookie();
    SetLogLevel(LogLevel::WARN);
    WithColor(errs(), BRIGHT_GREEN)
      << "Decoding successful!\n\n";
  }
  /*Encoding*/ {
    SmallStr<80> File;
    sys::path::append(File, ExamplePath, XmlFile);
    WithColor(errs(), BRIGHT_CYAN)
      << format("Encoding: \"{}\"", File.str()) << '\n';

    auto& Xml
      = Mgr->getOptXMLDocument(File.str(), errs())
        .expect("could not locate file!");
    
    SetLogLevel(LogLevel::WARN);
    Result EncoderOrErr = ExiEncoder::New(Opts);
    if (!EncoderOrErr) {
      errs() << EncoderOrErr.error() << '\n';
      WithColor(errs(), BRIGHT_RED)
        << "Encoding failed.\n";
      return 1;
    }

    ExiEncoder Encoder = std::move(*EncoderOrErr);
    XMLSerializer S(&Xml);
    Encoder.hdrHasCookie(HasCookie)
      .expect("Options already compiled??");
    Encoder.hdrHasOptions(HasOptions)
      .expect("Options already compiled??");

    SetLogLevel(CoderLogLevel);
    LOG_INFO("Compiling header...");
    SetLogLevel(LogLevel::INFO);
    Result Factory = Encoder.setup();
    if (!Factory) {
      errs() << Factory.error() << '\n';
      WithColor(errs(), BRIGHT_RED)
        << "Encoding failed.\n";
      return 1;
    }

    SetLogLevel(CoderLogLevel);
    LOG_INFO("Encoding body...");
    if (auto E = Factory->encode(&S, EncodeBuf)) {
      errs() << E << '\n';
      WithColor(errs(), BRIGHT_RED)
        << "Encoding failed.\n";
      return 1;
    }

    INFO_ONLY(dbgs() << '\n');
    SetLogLevel(LogLevel::WARN);
    WithColor(errs(), BRIGHT_GREEN)
      << "Encoding successful!\n\n";
  }
  /*Decoding, again*/ {
    WithColor(errs(), BRIGHT_CYAN)
      << format("Decoding: \"{}\"", OutExiFile) << '\n';
    
    if (!ExiFile.empty())
      PrintDiffOrCmp(DecodeBuf.getBuffer(), EncodeBuf.str());
    auto MB = MemoryBuffer::getMemBuffer(EncodeBuf.str(), OutExiFile,
                                         /*RequiresNullTerminator=*/false);

    SetLogLevel(LogLevel::WARN);
    ExiDecoder Decoder(Opts);
    XMLDeserializer S;

    SetLogLevel(CoderLogLevel);
    if (int Ret = Decode(Decoder, MB->getMemBufferRef(), &S)) {
      WithColor(errs(), BRIGHT_RED)
        << "Decoding failed.\n";
      return Ret;
    }

    SetLogLevel(LogLevel::WARN);
    WithColor(errs(), BRIGHT_GREEN)
      << "Decoding (again) successful!\n\n";
    
    if (Dump) {
      if (ExiS) {
        WithColor(errs(), BRIGHT_MAGENTA) << "Decoding result:\n";
        FullXMLDump(ExiS->document(), DO);
      }
      WithColor(errs(), BRIGHT_MAGENTA) << "Encoding result:\n";
      FullXMLDump(S.document(), DO);
    }
  }

  if (auto E = WriteFile(OutExiFile, EncodeBuf)) {
    logAllUnhandledErrors(std::move(E), errs());
    return 1;
  }

  if (Cmds && EXIFICIENT_DIR) {
    std::string Cmd;
    Cmd.reserve(120);
    raw_string_ostream OS(Cmd);

    constexpr StrRef JarName = "exificient-jar-with-dependencies.jar";
    OS << format("java -jar {}/{} -decode ", *EXIFICIENT_DIR, JarName.str());
    if (HasCookie)
      OS << "-includeCookie ";
    if (HasOptions)
      OS << "-includeOptions ";
    AddExificientCmdOpts(OS, Opts);
    sys::fs::make_absolute(FilenameStore);
    OS << format("-i \"{0}\" -o \"{0}.xml\"", FilenameStore.str());

    Cmds->emplace_back(std::move(Cmd));
  }
  
  return 0;
}

void ECDCTestRunner::run(StrRef ExiFile, StrRef XmlFile,
                         bool Diff, bool Dump, usize Skip) const {
  if (int Ret = runWithRet(ExiFile, XmlFile, Diff, Dump, Skip)) {
    if (Cmds)
      WriteExificientCmds(*Cmds);
    std::exit(Ret);
  }
}

#define MAKE_TEST_RUNNER(KIND, FOLDER, ...)                                   \
[&Mgr, &ExificientFileData] () -> ECDCTestRunner {                            \
  using enum exi::PreserveKind;                                               \
  const auto TheOpts = exi::make_preserve_opts(__VA_ARGS__);                  \
  return ECDCTestRunner(Mgr, FOLDER, KIND, TheOpts, &ExificientFileData);     \
}

/// Makes a test runner for "example".
#define MAKE_EXTEST_RUNNER(KIND, ...)                                         \
  MAKE_TEST_RUNNER(KIND, "example", ##__VA_ARGS__)

int main(int Argc, char* Argv[]) {
  using enum raw_ostream::Colors;
  SetLogLevel(LogLevel::WARN);
  HandleDebugSetup();
  const bool HaveEscapeCodes
    = HandleEscapeCodeSetup();
  outs().enable_colors(HaveEscapeCodes);
  errs().enable_colors(HaveEscapeCodes);
  dbgs().enable_colors(HaveEscapeCodes);

  XMLManagerRef Mgr = make_refcounted<XMLManager>();

#if STRESS_TEST_DECODING

  if (int Ret = TestSchemalessDecoding(Mgr)) {
    WithColor OS(outs(), BRIGHT_RED);
    OS << "Decoding failed.\n";
    return Ret;
  }

  WithColor OS(outs(), BRIGHT_GREEN);
  OS << "Decoding successful!\n";

#else

  // Add https://www.w3.org/TR/xmlschema-0/#ipo.xsd

  Vec<String> ExificientFileData;
  if (auto Dir = sys::Process::GetEnv("EXIFICIENT_DIR")) {
    SmallStr<80> FullPath;
    sys::path::append(FullPath, *Dir, "exificient-jar-with-dependencies.jar");
    if (sys::fs::exists(FullPath.str())) {
      EXIFICIENT_DIR = std::move(Dir);
      ExificientFileData.reserve(32);
    }
  }

  /*BytePacked*/ {
    auto Zil = MAKE_EXTEST_RUNNER(AlignKind::BytePacked);
    auto Pfx = MAKE_EXTEST_RUNNER(AlignKind::BytePacked, Prefixes);
    auto All = MAKE_EXTEST_RUNNER(AlignKind::BytePacked, All & ~LexicalValues);
    Zil().run("SpecExampleB.exi",     "SpecExample.xml");
    Zil().run("BasicNooptB.exi",      "Basic.xml");
    Zil().run("ThaiNooptB.exi",       "Thai.xml");
    Zil().run("StackedPNNooptB.exi",  "Stacked.xml");
    Pfx().run("CustomersNooptB.exi",  "Customers.xml");
    Pfx().run("StackedPPNooptB.exi",  "Stacked.xml");
    All().run("NamespaceNooptB.exi",  "Namespace.xml");
  }
  ExificientFileData.push_back("");
  /*BitPacked*/ {
    auto Zil = MAKE_EXTEST_RUNNER(AlignKind::BitPacked);
    auto Pfx = MAKE_EXTEST_RUNNER(AlignKind::BitPacked, Prefixes);
    auto All = MAKE_EXTEST_RUNNER(AlignKind::BitPacked, All & ~LexicalValues);
    Zil().run("SpecExample.exi",      "SpecExample.xml");
    Zil().run("BasicNoopt.exi",       "Basic.xml");
    Zil().run("StackedPNNoopt.exi",   "Stacked.xml");
    Zil().run("ThaiNoopt.exi",        "Thai.xml");
    Pfx().run("CustomersNoopt.exi",   "Customers.xml");
    Pfx().run("StackedPPNoopt.exi",   "Stacked.xml");
    Pfx().run("OrdersSmall.exi",      "OrdersSmall.xml");
    All().run("NamespaceNoopt.exi",   "Namespace.xml");
#if TEST_LARGE_EXAMPLES
    SetLogLevel(LogLevel::WARN);
    Pfx().run("Orders.exi", "Orders.xml", /*Diff=*/false);
#endif
  }

  if (EXIFICIENT_DIR.has_value()) {
    WriteExificientCmds(ExificientFileData);
    errs() << "Running exificient...\n";
    /// HACK: This isn't great, replace it with better impl.
#if EXI_ON_WIN32
    std::system(".\\examples\\out\\ExificientCmds.bat "
             "2> .\\examples\\out\\ExificientCmds.log");
#else
    std::system("./examples/out/ExificientCmds.sh "
             "2> ./examples/out/ExificientCmds.log");
#endif
    WithColor(errs(), BRIGHT_GREEN)
      << "Wrote to 'examples/out/ExificientCmds.*'\n\n";
  }

#endif
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
  using enum raw_ostream::Colors;
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
      .Preserve = Preserve,
      .SchemaID = Some(nullptr)
    });
  };

  auto DecodePreserveBytes = [&DecodeFile]
   (StrRef HiddenFile, ExiOptions::PreserveOpts Preserve = {}) {
    return DecodeFile(HiddenFile, {
      .Alignment = AlignKind::BytePacked,
      .Preserve = Preserve,
      .SchemaID = Some(nullptr)
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
    SetLogLevel(LogLevel::VERBOSE);
    // SpecExample.xml with default settings and no options.
    // The example data provided by EXI.
    DECODE_ORD_BITS("SpecExample.exi");
    DECODE_ORD_BYTES("SpecExampleB.exi");

    SetLogLevel(LogLevel::INFO);
    // Basic.xml with default settings and no options.
    DECODE_ORD_BITS("BasicNoopt.exi");
    DECODE_ORD_BYTES("BasicNooptB.exi");

    // Customers.xml with Preserve.prefixes and no options.
    // Small namespace example.
    DECODE_ORD_BITS("CustomersNoopt.exi",   Prefixes);
    DECODE_ORD_BYTES("CustomersNooptB.exi", Prefixes);
  }

  SetLogLevel(LogLevel::VERBOSE);
  // Thai.xml with default settings and no options.
  // Unicode string example.
  DECODE_ORD_BITS("ThaiNoopt.exi");
  DECODE_ORD_BYTES("ThaiNooptB.exi");

  SetLogLevel(LogLevel::INFO);
  // Namespace.xml with all content and no options.
  // All features (minus lexical values) tested.
  DECODE_ORD_BITS("NamespaceNoopt.exi",   All & ~LexicalValues);
  DECODE_ORD_BYTES("NamespaceNooptB.exi", All & ~LexicalValues);

#if defined(NDEBUG) && TEST_LARGE_EXAMPLES
  SetLogLevel(LogLevel::WARN);
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
