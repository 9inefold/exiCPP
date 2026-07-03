//===- Driver.cpp ---------------------------------------------------===//
//
// Copyright (C) 2024-2026 Ninefold
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
#include <Common/ConstexprLiteral.hpp>
#include <Common/EnumArray.hpp>
#include <Common/RefCntPtr.hpp>
#include <Common/MMatch.hpp>
#include <Common/MaybeBox.hpp>
#include <Common/PointerIntPair.hpp>
#include <Common/Poly.hpp>
#include <Common/Result.hpp>
#include <Common/SmallStr.hpp>
#include <Common/StringSwitch.hpp>
#include <Common/Twine.hpp>

#include <Demangle/Demangle.hpp>
#include <Demangle/ItaniumDemangle.hpp>

#include <Support/BinaryToText.hpp>
#include <Support/Filesystem.hpp>
#include <Support/InitDriver.hpp>
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
#include <exi/Basic/XMLContainer.hpp>
#include <exi/Basic/XMLCompare.hpp>
#include <exi/Basic/XMLManager.hpp>
#include <exi/Stream/OrderedReader.hpp>
#include <exi/Stream/OrderedWriter.hpp>

#include <exi/Encode/BodyEncoder.hpp>
#include <exi/Encode/NamespaceContextStack.hpp>
#include <exi/Encode/XMLSerializer.hpp>

#include <exi/Decode/BodyDecoder.hpp>
#include <exi/Decode/StreamDeserializer.hpp>
#include <exi/Decode/XMLDeserializer.hpp>

#include <algorithm>
#include <dtl/dtl.hpp>
#include <rapidxml.hpp>

#define DEBUG_TYPE "__DRIVER__"
#define TEST_LARGE_EXAMPLES 0
#define STRESS_TEST_DECODING 0

using namespace exi;

static constinit Option<String> EXIFICIENT_DIR = std::nullopt;

ALWAYS_INLINE static constexpr 
 void SetLogLevel([[maybe_unused]] LogLevelType NewLevel) {
#if EXI_LOGGING
  exi::DebugFlag = NewLevel;
#endif
}

static void RunDumps(XMLManager& Mgr, bool Conforming = false) {
  using namespace root;
  DumpOptions Opts {.Conforming = Conforming};
  XMLDump::full(Mgr, "examples/022.xml", Opts);
  XMLDump::full(Mgr, "examples/044.xml", Opts);
  XMLDump::full(Mgr, "examples/079.xml", Opts);
  XMLDump::full(Mgr, "examples/085.xml", Opts);
  XMLDump::full(Mgr, "examples/103.xml", Opts);
  XMLDump::full(Mgr, "examples/116.xml", Opts);
  XMLDump::full(Mgr, "examples/Namespace.xml", Opts);
  XMLDump::full(Mgr, "examples/SortTest.xml", Opts);
  XMLDump::full(Mgr, "examples/Thai.xml", Opts);

  // Without prints this runs in 0.2 seconds!
  // XMLDump::full(Mgr, "large-examples/treebank_e.xml", Opts);
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

static ExiError DecodeBody(ExiDecoder& Decoder, Deserializer* S = nullptr) {
  if (S)
    return Decoder.decodeBody(S);
  else
    return Decoder.decodeBody();
}

static int Decode(ExiDecoder& Decoder, UnifiedBuffer MB, Deserializer* S = nullptr) {
  LOG_INFO("Decoding header...");
  if (auto E = Decoder.decodeHeader(MB.buf())) {
    errs() << E << '\n';
    return 1;
  }

  LOG_INFO("Decoding body...");
  if (auto E = DecodeBody(Decoder, S)) {
    errs() << E << '\n';
    return 1;
  }

  INFO_ONLY(dbgs() << '\n');
  return 0;
}

static int Decode(UnifiedBuffer MB, ExiOptions& Opts, Deserializer* S = nullptr) {
  LOG_INFO("Decoding: \"{}\"", MB.buf().getBufferIdentifier());
  ExiDecoder Decoder(Opts);
  return Decode(Decoder, MB, S);
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

static int Encode(ExiEncoder& Encoder, XMLSerializer& S, SmallVecImpl<char>& EncodeBuf) {
  LOG_INFO("Compiling header...");
  Result Factory = Encoder.setup();
  if (!Factory) {
    errs() << Factory.error() << '\n';
    return 1;
  }

  LOG_INFO("Encoding body...");
  EncodeBuf.clear();
  if (auto E = Factory->encode(&S, EncodeBuf)) {
    errs() << E << '\n';
    return 1;
  }

  INFO_ONLY(dbgs() << '\n');
  return 0;
}

static int Encode(ExiEncoder& Encoder, XMLSerializer& S) {
  SmallStr<0> EncodeBuf;
  return Encode(Encoder, S, EncodeBuf);
}

static int Encode(XMLDocument& Xml, ExiOptions& Opts, ExiHeaderOnly Hdr = {},
                  SmallVecImpl<char>* EncodeBuf = nullptr) {
  LOG_INFO("Encoding: \"{}\"", Xml.identifier());
  Result EncoderOrErr = ExiEncoder::New(Opts);
  if (!EncoderOrErr) {
    errs() << EncoderOrErr.error() << '\n';
    return 1;
  }

  ExiEncoder Encoder = std::move(*EncoderOrErr);
  Encoder.setHeaderOnly(Hdr)
    .expect("Options already compiled??");

  XMLSerializer S(Xml);
  S.PreserveCDATA = PreserveCDATAKind::CDATA_PRESERVE;
  //S.SkipEmptyCH = true;

  if (EncodeBuf)
    return Encode(Encoder, S, *EncodeBuf);
  else
    return Encode(Encoder, S);
}

static int Encode(XMLManager* Mgr, StrRef File,
                  ExiOptions& Opts, ExiHeaderOnly Hdr = {},
                  SmallVecImpl<char>* EncodeBuf = nullptr) {
  XMLDocument& Xml
    = Mgr->getOptXMLDocument(File, errs())
      .expect("could not locate file!");
  return Encode(Xml, Opts, Hdr, EncodeBuf);
}

static int Encode(XMLManager* Mgr, StrRef File, ExiHeader& HdrOpts,
                  SmallVecImpl<char>* EncodeBuf = nullptr) {
  ExiHeaderOnly Hdr = exi::GetHeaderOnlyData(HdrOpts);
  if (!HdrOpts.Opts) {
    errs() << File << ": Options not provided!\n";
    return 1;
  }
  return Encode(Mgr, File, *HdrOpts.Opts, Hdr, EncodeBuf);
}

static int EncodeNoOpts(XMLManager* Mgr, StrRef File, ExiOptions& Opts,
                        SmallVecImpl<char>* EncodeBuf = nullptr) {
  ExiHeaderOnly Hdr { .HasOptions = false };
  return Encode(Mgr, File, Opts, Hdr, EncodeBuf);
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

static void PadByteDiffViewerStr(StrRef Original, StrRef Encoded,
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

static void PadByteDiffViewer(UnifiedBuffer Original, UnifiedBuffer Encoded,
                              usize BreakOn = 4, bool Hex = false,
                              bool LabelX = false, usize SkipY = 0) {
  return PadByteDiffViewerStr(Original.str(), Encoded.str(),
                              BreakOn, Hex, LabelX, SkipY);
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
struct CompareMetadata {
  Vec<String> RawCommands;
  Vec<std::tuple<String, String, ExiOptions::PreserveOpts>> OldToNewMapping;
public:
  void reserve(usize N) {
    RawCommands.reserve(N);
    OldToNewMapping.reserve(N);
  }
};

/// EnCode/DeCode Test Runner
class ECDCTestRunner {
  XMLManagerRef Mgr;
  StrRef ExamplePath = "examples";
  mutable ExiOptions Opts;
  CompareMetadata* Cmds = nullptr;

public:
  ECDCTestRunner(XMLManagerRef Mgr, StrRef Folder,
                 AlignKind K, ExiOptions::PreserveOpts P,
                 CompareMetadata* Cmds = nullptr)
   : Mgr(std::move(Mgr)), ExamplePath(Folder), Opts{
      .Alignment = K, .Preserve = P,
      .SchemaID = Some(nullptr)},
     Cmds(Cmds) {
  }

  ECDCTestRunner(XMLManagerRef Mgr,
                 AlignKind K, ExiOptions::PreserveOpts P,
                 CompareMetadata* Cmds = nullptr)
   : ECDCTestRunner(std::move(Mgr), "examples", K, P, Cmds) {
  }

  /// Runs Decoder -> Encoder -> Decoder.
  int runWithRet(StrRef ExiFile, StrRef XmlFile,
                 bool Diff = false, bool Dump = false, usize Skip = 0) const;
  /// Invokes run, exits on failure.
  void run(StrRef ExiFile, StrRef XmlFile,
           bool Diff = false, bool Dump = false, usize Skip = 0) const;
  /// Invokes run without exi, exits on failure.
  void runXml(StrRef XmlFile, bool Diff = false,
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

static void WriteExificientCmds(const CompareMetadata& Contents) {
  return WriteExificientCmds(Contents.RawCommands);
}

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

static constexpr auto kPreserveCDATA = PreserveCDATAKind::CDATA_NONE;
//static constexpr auto kPreserveCDATA = PreserveCDATAKind::CDATA_PRESERVE;

int ECDCTestRunner::runWithRet(StrRef ExiFile, StrRef XmlFile,
                               bool Diff, bool Dump, usize Skip) const {
  using enum raw_ostream::Colors;
  auto CoderLogLevel = exi::DebugFlag;
  root::DumpOptions DO {
    .Conforming = true,
    .PreserveDeclaration = false,
    .PreserveCDATA = kPreserveCDATA
  };
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

  XMLDocument* Xml = nullptr;
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

  ExiHeaderOnly HdrOnlyOpts {.HasOptions = false};
  const bool& HasCookie = HdrOnlyOpts.HasCookie;
  const bool& HasOptions = HdrOnlyOpts.HasOptions;

  WithColor(errs(), BRIGHT_MAGENTA) << "BEGINNING RUN:\n";

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
    //S.PreserveCDATA = PreserveCDATAKind::CDATA_ESCAPE;
    S.PreserveCDATA = PreserveCDATAKind::CDATA_NONE;
    //S.SkipEmptyCH = true;

    SetLogLevel(CoderLogLevel);
    if (int Ret = Decode(Decoder, MB, &S)) {
      WithColor(errs(), BRIGHT_RED)
        << "Decoding failed.\n";
      return Ret;
    }

    SetLogLevel(LogLevel::WARN);
    auto HOOptsOrErr = Decoder.headerOnly();
    if (HOOptsOrErr.is_err()) {
      WithColor(errs(), BRIGHT_RED)
        << "Decoding failed? "
        << HOOptsOrErr.error() << '\n';
      return 1;
    }

    HdrOnlyOpts = *HOOptsOrErr;
    WithColor(errs(), BRIGHT_GREEN)
      << "Decoding successful: "
      << exi_mangle_header(Decoder.header()) << "!\n\n";
  }
  /*Encoding*/ {
    SmallStr<80> File;
    sys::path::append(File, ExamplePath, XmlFile);
    WithColor(errs(), BRIGHT_CYAN)
      << format("Encoding: \"{}\"", File.str()) << '\n';

    Xml = &Mgr->getOptXMLDocument(File.str(), errs())
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
    Encoder.setHeaderOnly(HdrOnlyOpts)
      .expect("Options already compiled??");
    //Encoder.hdrHasCookie(HasCookie)
    //  .expect("Options already compiled??");
    //Encoder.hdrHasOptions(HasOptions)
    //  .expect("Options already compiled??");

    XMLSerializer S(Xml);
    S.PreserveCDATA = kPreserveCDATA;
    //S.SkipEmptyCH = true;

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
    
    if (!ExiFile.empty()) {
      PrintDiffOrCmp(DecodeBuf.getBuffer(), EncodeBuf.str());
      errs() << '\n';
    }
  }
  /*Decoding, again*/ {
    WithColor(errs(), BRIGHT_CYAN)
      << format("Decoding: \"{}\"", OutExiFile) << '\n';
    auto MB = MemoryBuffer::getMemBuffer(EncodeBuf.n_str(), OutExiFile,
                                         /*RequiresNullTerminator=*/false);

    SetLogLevel(LogLevel::WARN);
    ExiDecoder Decoder(Opts);
    XMLDeserializer S;
    S.PreserveCDATA = kPreserveCDATA;
    S.SkipEmptyCH = true;

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
      auto CompareXml = [Xml] (XMLDocument* Other, SmallVecImpl<char>& V) {
        if (!Xml)
          return true;
        raw_svector_ostream OS(V);
        return compareXML(Xml, Other, OS);
      };
      auto Dump = [&DO] (XMLDocument& Doc) {
        WithColor OS(errs(), BRIGHT_WHITE);
        //XMLDump::full(Doc, DO);
        XMLDump::raw(Doc, DO);
        //XMLDump::info_tree(Doc, DO);
      };

      SmallStr<0> DErr, EErr;
      bool DOk = !ExiS || CompareXml(&ExiS->document(), DErr);
      bool EOk = CompareXml(&S.document(), EErr);

      if (Xml) {
        WithColor(errs(), MAGENTA) << "Original:\n";
        Dump(*Xml);
      }
      if (ExiS && !DOk) {
        WithColor OS(errs(), BRIGHT_RED);
        OS << "Decoding result:\n" << DErr.str();
        Dump(ExiS->document());
      }
      if (!EOk) {
        WithColor OS(errs(), BRIGHT_RED);
        OS << "Encoding result:\n" << EErr.str();
        Dump(S.document());
      }
    } else if (Xml) {
      using elem = StrRef;
      using sequence = SmallVec<elem, 0>;

      auto Dump = [P = Opts.Preserve] (XMLDocument& D, SmallVecImpl<char>& V) {
        raw_svector_ostream OS(V);
        OS.enable_colors(false);
        XMLDump::full(D, XMLDumpOptions {
          .OS                   = OS,
          .InitialIndent        = 0,
          .Conforming           = false,
          .PrintRawNames        = P.Prefixes,
          .Preserve             = P,
          .PreserveDeclaration  = false,
          .PreserveCDATA        = kPreserveCDATA
        });
      };
      auto Diff = [] (sequence& LHS, sequence& RHS) {
        dtl::Diff<elem> diff(LHS, RHS);
        diff.onHuge();
        diff.compose();

        if (diff.getEditDistance() == 0)
          return;
        
        WithColor OS(errs(), raw_ostream::BRIGHT_WHITE);
        diff.composeUnifiedHunks();
        diff.printUnifiedFormat(OS);
      };

      SmallStr<0> Og, Enc, Dec;
      Dump(*Xml, Og);
      Dump(S.document(), Enc);
      if (ExiS)
        Dump(ExiS->document(), Dec);

      sequence OgLines, EncLines, DecLines;
      Og.str().split(OgLines, '\n', -1, false);

      if (ExiS && Og.str() != Dec.str()) {
        Dec.str().split(DecLines, '\n', -1, false);
        WithColor(errs(), RED) << "Decoding result:\n";
        Diff(OgLines, DecLines);
      }
      if (Og.str() != Enc.str()) {
        Enc.str().split(EncLines, '\n', -1, false);
        WithColor(errs(), RED) << "Encoding result:\n";
        Diff(OgLines, EncLines);
      }
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
    // OS << "-retainEntityReference ";
    if (HasCookie)
      OS << "-includeCookie ";
    if (HasOptions)
      OS << "-includeOptions ";
    AddExificientCmdOpts(OS, Opts);
    sys::fs::make_absolute(FilenameStore);
    OS << format("-i \"{0}\" -o \"{0}.xml\"", FilenameStore.str());
#if 0
    Cmds->RawCommands.emplace_back(std::move(Cmd));
    //if (Opts.Preserve.Prefixes) {
      Cmds->OldToNewMapping.emplace_back(
        fmt::format("{}/{}", ExamplePath, XmlFile.str()),
        fmt::format("{}.xml", FilenameStore.str()),
        this->Opts.Preserve
      );
    //}
#endif
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

/// Makes a test runner for "examples".
#define MAKE_EXTEST_RUNNER(KIND, ...)                                         \
  MAKE_TEST_RUNNER(KIND, "examples", ##__VA_ARGS__)

static bool DemangleTests(bool Print = false) {
  raw_ostream& OS = errs_if(Print);
  {
    ExiOptions Opts;
    if (!exi::exi_demangle_options(Opts, "iPcdlipV0"))
      return false;
    OS << exi::exi_mangle_options(Opts) << '\n';

    Opts.SchemaID = std::make_unique<std::string>("BeerXML.xsd");
    auto Mangled = exi::exi_mangle_options(Opts);
    OS << Mangled << '\n';

    ExiOptions Opts2;
    if (!exi::exi_demangle_options(Opts2, Mangled))
      return false;
    exi_relassert(Opts2.SchemaID);
    auto& SchemaID = *Opts2.SchemaID;
    exi_relassert(SchemaID);
    OS << '\"' << *SchemaID << "\"\n";
  }

  {
    SmallStr<32> EBuf, DBuf;
    StrRef Original = "BeerXML.xsd";
    StrRef Encoded = zbase32::encode(Original, EBuf);
    Expected<StrRef> DecodedOrErr = zbase32::decode(Encoded, DBuf);
    if (!DecodedOrErr) {
      exi::String Err = toString(DecodedOrErr.takeError());
      OS << "Error decoding: " << Err << '\n';
      return false;
    }
    StrRef Decoded = *DecodedOrErr;
    if (Print) {
      outs() << "Original: " << Original << '\n';
      outs() << "Encoded:  " << Encoded << '\n';
      outs() << "Decoded:  " << Decoded << '\n';
    }
    exi_relassert(Original == Decoded);
  }

  return true;
}

static bool isExiFile(XMLContainerRef FileData, raw_ostream* OS = nullptr) {
  auto Outs = [&FileData, OS] () -> raw_ostream& {
    if (!OS)
      return nulls();
    return *OS << FileData.getRelativeName() << ": "_str;
  };

  using enum XMLKind;
  if (!FileData.hasBuffer()) {
    Outs() << "file does not have buffer!\n";
    return false;
  }
  // Check if it is an exi type
  const XMLKind K = FileData.getKind();
  if (MMatch(K).is(ExiDocument, XsdExiSchema)) {
    if (K == XsdExiSchema)
      LOG_WARN("XsdExiSchema loaded as ExiDocument.");
    return true;
  }
  // Check the extension...
  StrRef FullName = FileData.getName();
  if (K != XMLKind::Unknown || !FullName.ends_with_insensitive(".exi")) {
    Outs() << "invalid file type!\n";
    return false;
  }
  return true;
}

static constinit StrRef TestOutputFolder = "tests/o2";

/// @returns the number of failed tests
static int LoadExiTestFilesCallback(XMLManagerRef Mgr, UniqueStringSaver& Saver,
                                    function_ref<void(MemoryBufferRef)> Output,
                                    StrRef SubFolder, ArrayRef<StrRef> Files) {
  auto Folder = fmt::format("{}/{}/", TestOutputFolder, SubFolder);
  raw_ostream& OS = errs();
  int NumFailed = 0;
  for (StrRef File : Files) {
    Option<XMLContainerRef> OptFileData
      = Mgr->getOptXMLRef(Twine(Folder) + File, OS);
    // Check if we need a fake file.
    if (!OptFileData.has_value() || !isExiFile(*OptFileData, &OS)) {
      StrRef FullPath = Saver.save(Twine(Folder) + File);
      Output(MemoryBufferRef("", FullPath));
      ++NumFailed;
      continue;
    }
    // Everything went smoothly!
    Output(OptFileData->getBufferRef());
  }
  return NumFailed;
}

/// @returns the number of failed tests
static int LoadExiTestFiles(XMLManagerRef Mgr, UniqueStringSaver& Saver,
                            std::vector<MemoryBufferRef>& Output,
                            StrRef SubFolder, ArrayRef<StrRef> Files) {
  auto OutputFn = [&] (MemoryBufferRef MB) { Output.push_back(MB); };
  return LoadExiTestFilesCallback(Mgr, Saver, OutputFn, SubFolder, Files);
}
/// @returns the number of failed tests
static int LoadExiTestFiles(XMLManagerRef Mgr, UniqueStringSaver& Saver,
                            SmallVecImpl<MemoryBufferRef>& Output,
                            StrRef SubFolder, ArrayRef<StrRef> Files) {
  auto OutputFn = [&] (MemoryBufferRef MB) { Output.push_back(MB); };
  return LoadExiTestFilesCallback(Mgr, Saver, OutputFn, SubFolder, Files);
}

void doTestTests(RefCntPtr<XMLManager> Mgr) {
  auto LoadExiBuffer = [Mgr] (const Twine& Path) -> MemoryBufferRef {
    raw_ostream& OS = errs();
    XMLContainerRef File
      = Mgr->getOptXMLRef(Twine(TestOutputFolder) + "/" + Path, OS)
        .expect("could not locate file!");
    // Validate the type
    exi_relassert(isExiFile(File, &OS));
    return File.getBufferRef();
  };

  BumpPtrAllocator NameAlloc;
  UniqueStringSaver NameSaver(NameAlloc);

  auto LoadExiTests = [&] (StrRef SubFolder, ArrayRef<StrRef> Files) {
    SmallVec<MemoryBufferRef> Out;
    int Failed = LoadExiTestFiles(Mgr, NameSaver, Out, SubFolder, Files);
    if (Failed > 0)
      LOG_WARN("Failed to load {} files", Failed);
    return Out;
  };

  auto PrintBreak = [] () {
    if (!hasDbgLogLevel(INFO)) {
      errs() << '\n';
      return;
    }
    WithColor(errs(), raw_ostream::BRIGHT_MAGENTA)
      << "\n**********************************************************************\n";
  };

  //{{
  //  auto MBo = LoadExiBuffer("ch.ch-01/iPc.o.exi");
  //  auto MBi = LoadExiBuffer("ch.ch-01/iPc.i.exi");
  //  PadByteDiffViewer(MBo.getBuffer(), MBi.getBuffer());
  //}
  //{
  //  auto MBo = LoadExiBuffer("ch.ch-01/yPc.o.exi");
  //  auto MBi = LoadExiBuffer("ch.ch-01/yPc.i.exi");
  //  PadByteDiffViewer(MBo.getBuffer(), MBi.getBuffer(), 8, true);
  //}}

  ExiOptions Opts { .SchemaID = Some(nullptr) };
  ExiHeaderOnly Hdr { .HasOptions = false };
  SmallStr<0> EncodeBuf;
  
  {
    //SetLogLevel(LogLevel::EXTRA);
    //exi_demangle_options(Opts, "iPc");

    //PrintBreak();
    //Decode(Mgr.get(), "tests/o2/ch.ch-01/iPc.o.exi", Opts);
    //PrintBreak();
    //Encode(Mgr.get(), "tests/o2/ch.ch-01/ch-01.xml", Opts, Hdr);
    ////PrintBreak();
    ////Decode(Mgr.get(), "tests/o2/ch.ch-01/iPc.i.exi", Opts);
  }

  SetLogLevel(LogLevel::EXTRA);

  auto DoEncodeOnly = [&] (StrRef SubFolder, StrRef Mangling,
                           bool DecodeAfter = true) -> int {
    auto [Folder, Entry] = SubFolder.split('.');
    auto xml = fmt::format("tests/s/{}/{}.xml", Folder, Entry);

    PrintBreak();
    exi_try(Encode(Mgr.get(), xml, Opts, Hdr, &EncodeBuf));

    if (DecodeAfter) {
      PrintBreak();
      //exi_try(Decode(EncodeBuf.str(), Opts));
      XMLDeserializer XD;
      //XD.UURIType = UnboundURIKind::UURI_OPENEXI;
      //XD.UURIType = UnboundURIKind::UURI_EXIFICIENT;
      exi_try(Decode(EncodeBuf.str(), Opts, &XD));
      //XMLDump::full(XD.document(), {.Conforming = true});
      XMLDump::raw(XD.document(), {.Conforming = true});
    }

    return 0;
  };

  auto CheckOutput = [&] (StrRef SubFolder, StrRef Mangling,
                          bool DecodeAfter = false) -> int {
    exi_demangle_options(Opts, Mangling);
    const bool IsHex = (Opts.Alignment != AlignKind::BitPacked);
    const int BreakOn = IsHex ? 8 : 4;

    auto oexi = fmt::format("{}/{}.o.exi", SubFolder, Mangling);
    auto iexi = fmt::format("{}/{}.i.exi", SubFolder, Mangling);

    auto MBo = LoadExiBuffer(oexi);
    auto MBi = LoadExiBuffer(iexi);
    PadByteDiffViewer(MBo, MBi, BreakOn, IsHex);

    PrintBreak();
    exi_try(Decode(MBo, Opts));

    PrintBreak();
    exi_try(Decode(MBi, Opts));

    exi_try(DoEncodeOnly(SubFolder, Mangling, /*DecodeAfter=*/false));
    PadByteDiffViewer(MBo, EncodeBuf.str(), BreakOn, IsHex);

    if (DecodeAfter) {
      PrintBreak();
      exi_try(Decode(EncodeBuf.str(), Opts));
      XMLDeserializer XD;
      //XD.UURIType = UnboundURIKind::UURI_OPENEXI;
      //XD.UURIType = UnboundURIKind::UURI_EXIFICIENT;
      //exi_try(Decode(EncodeBuf.str(), Opts, &XD));
      XMLDump::full(XD.document(), {.Conforming = true});
    }

    return 0;
  };

  //StrRef Mangling = "iPcdi";
  //StrRef Mangling = "yPp";
  //exi_demangle_options(Opts, Mangling);
  //auto MBo = LoadExiBuffer("at.at-02/" + Twine(Mangling) + ".o.exi");
  //if (Decode(MBo, Opts))
  //  return;

  //CheckOutput("el.el-01", "yPcdi");
  //CheckOutput("ch.ch-01", "yPc");
  //DoEncodeOnly("el2.el2-07", "iPcdi");
  //DoEncodeOnly("el2.el2-09", "i");
  //DoEncodeOnly("at.at-01", "iPcdi");

  //CheckOutput("at.at-02", "iPcdi");
  //DoEncodeOnly("at.at-02", "iPcdi");
  DoEncodeOnly("at.at-02", "iPcdip");
  //DoEncodeOnly("at.at-02", "i");

  // TODO: Support universal names in decoding
  //DoEncodeOnly("el2.el2-07a", "iPpcdi");

  /*{
    SetLogLevel(LogLevel::EXTRA);
    exi_demangle_options(Opts, "yPc");

    PrintBreak();
    auto yPcMBo = LoadExiBuffer("ch.ch-01/yPc.o.exi");
    Decode(yPcMBo, Opts);

    PrintBreak();
    Encode(Mgr.get(), "tests/o2/ch.ch-01/ch-01.xml", Opts, Hdr, &EncodeBuf);
    PadByteDiffViewer(yPcMBo.getBuffer(), EncodeBuf.str(), 8, true);

    PrintBreak();
    auto yPcMBi = LoadExiBuffer("ch.ch-01/yPc.i.exi");
    PadByteDiffViewer(yPcMBo.getBuffer(), yPcMBi.getBuffer(), 8, true);
    Decode(yPcMBi, Opts);
  }*/
}

struct DemanglingInfoCorrectnessTestCase {
  std::string_view mangled;
  std::string_view demangled;
};

// Returns [Failed, Total]
static std::pair<usize, usize> RunDemangleTests() {
  static constexpr DemanglingInfoCorrectnessTestCase DemangleCases[] {
  // Hide this from intellisense
#if !EXI_IS_LANG_SERVER
# include <core/Demangle/TestCases.mac>
#endif
  };
  // Run the tests
  const usize Total = usize(std::size(DemangleCases));
  SmallVec<std::pair<const DemanglingInfoCorrectnessTestCase*, char*>> FailedCases;
  usize Count = 0;
  errs() << '\n';
  for (const DemanglingInfoCorrectnessTestCase& Case : DemangleCases) {
    if ((Count & 0b1111) == 0)
      errs() << "Test " << Count << "/" << Total << "\r";
    auto [Mangled, Demangled] = Case;
    char* Result = exi::itaniumDemangle(Mangled);
    if (!Result) {
      FailedCases.push_back({&Case, nullptr});
    } else {
      if (std::string_view(Result) == Demangled)
        exi::exi_free(Result);
      else
        FailedCases.push_back({&Case, Result});
    }
    ++Count;
  }
  errs() << "Test " << Count << "/" << Total << "\n";

  for (auto& [Case, Result] : FailedCases) {
    auto [Mangled, Demangled] = *Case;
    errs() << "For " << Mangled
           << "\n  Expected: " << Demangled
           << "\n  Got     : ";
    if (Result) {
      errs() << Result << "\n";
      exi::exi_free(Result);
    } else
      errs() << "<nothing>\n";
  }
  return {FailedCases.size(), Total};
}

int main(int Argc, char* Argv[]) {
  using enum raw_ostream::Colors;
  SetLogLevel(LogLevel::WARN);
  InitDriver X(Argc, Argv);
  sys::UsePrettyStackTraceOptions();

  //const XMLParseOptions ParseOpts { .MergeData = true };
  const XMLParseOptions ParseOpts { .MergeData = false };
  XMLManagerRef Mgr = make_refcounted<XMLManager>(ParseOpts);

  //outs() << "Is debugging: " << sys::Process::IsReallyDebugging() << '\n';

  using HelloWorld = make_charseq<"Hello world!">;
  static_assert(matches_seq<HelloWorld>("Hello world!!"));

  static constexpr char CDATA_EX[] = "<![CDATA";
  static_assert(matches_seq<"<![">(CDATA_EX)
             && matches_seq<"CDATA", 3>(CDATA_EX));

#if 1
  {
    errs() << "Running tests...\n";
    auto [Failed, Total] = RunDemangleTests();
    errs() << "Passed " << (Total - Failed) << "/" << Total << " demangling tests.\n\n";
    if (Failed > 0)
      return 1;
  }
#endif

  std::string_view OtherTests[] = {
    //"_ZZL9SortAttrsPKN3xml7XMLNodeIcEERN3exi12SmallVecImplIPKNS_12XMLAttributeIcEEEEPSA_ENK3$_0clIS8_S8_EEbPT_PT0_",
    "_ZZ1fvEN3$_0clI1SS1_EEbT_T0_",
    "_ZZ2f1vEN3$_1clIK1SEEbPT_S4_",
    "_ZZ2f2vEN3$_0clI1SS1_EEbT_T0_",
    "_ZZ1tIV1SEvvEN3$_2clIS0_EEbS0_T_",
    "_ZZZ1tIV1SEvvEN3$_2clIS0_EEbS0_T_ENUlS4_T0_E_clIS0_S0_EEbS4_S5_",
    "_ZZ2ffvEN3$_1clIZZ2ffvEN3$_0clI1SEEDaT_E1SZZ2ffvENS2_IS5_EEDaS4_E1SEEbS4_T0_",
  };

  for (auto Test : OtherTests) {
    char* Output = exi::itaniumDemangle(Test);
    if (!Output)
      fmt::println(stderr, "{}: FAILED", Test);
    else {
      fmt::println(stderr, "{}: {}", Test, Output);
      exi::exi_free(Output);
    }
  }

  errs() << "Done!\n";

#if 1
  //doTestTests(Mgr);
  return 0;
#endif

#if STRESS_TEST_DECODING

  if (int Ret = TestSchemalessDecoding(Mgr)) {
    WithColor OS(outs(), BRIGHT_RED);
    OS << "Decoding failed.\n";
    return Ret;
  }

  WithColor OS(outs(), BRIGHT_GREEN);
  OS << "Decoding successful!\n";

#else

#if 0

  auto TryDump = [&Mgr, Embed = ParseOpts.MergeData] (const Twine& Path) {
    WithColor OS(outs(), raw_ostream::BRIGHT_WHITE);
    XMLDumpOptions DumpOpts {
      .InitialIndent        = 0,
      .Conforming           = false,
      .PreserveDeclaration  = false,
      .PreserveCDATA        = PreserveCDATAKind::CDATA_NONE,
      .EmbeddedCDATA        = Embed
    };
    //XMLDump::full(*Mgr, Path, DumpOpts);
    XMLDump::raw(*Mgr, Path, DumpOpts);
  };

  TryDump("examples/at-01.xml");
  TryDump("tests/s/me/Newlines2.xml");
  TryDump("tests/s/me/CDATA2.xml");

  return 0;

#endif

  // Add https://www.w3.org/TR/xmlschema-0/#ipo.xsd

  CompareMetadata ExificientFileData;
  if (auto Dir = sys::Process::GetEnv("EXIFICIENT_DIR")) {
    SmallStr<80> FullPath;
    sys::path::append(FullPath, *Dir, "exificient-jar-with-dependencies.jar");
    if (sys::fs::exists(FullPath.str())) {
      EXIFICIENT_DIR = std::move(Dir);
      ExificientFileData.reserve(32);
    }
    // TODO: Add defaulted jar
  }

#if 0
  /*BytePacked*/ {
    auto Zil = MAKE_EXTEST_RUNNER(AlignKind::BytePacked);
    auto Pfx = MAKE_EXTEST_RUNNER(AlignKind::BytePacked, Prefixes);
    auto All = MAKE_EXTEST_RUNNER(AlignKind::BytePacked, All & ~LexicalValues);
    //All().run("CDATANooptB.exi",      "CDATA.xml", false, true);
    All().run("116NooptB.exi",        "116.xml", false, true);
    return 0;
    Zil().run("SpecExampleB.exi",     "SpecExample.xml");
    Zil().run("BasicNooptB.exi",      "Basic.xml");
    Zil().run("ThaiNooptB.exi",       "Thai.xml");
    Zil().run("StackedPNNooptB.exi",  "Stacked.xml");
    Pfx().run("CustomersNooptB.exi",  "Customers.xml");
    Pfx().run("StackedPPNooptB.exi",  "Stacked.xml");
    All().run("NamespaceNooptB.exi",  "Namespace.xml");
    All().run("CDATANooptB.exi",      "CDATA.xml", false, true);

    All().run("el2-05.xml.exi",       "el2-05.xml", false, true);
    All().run("el2-05r.xml.exi",      "el2-05r.xml");
    All().run("el2-07.xml.exi",       "el2-07.xml", false, true);
    All().run("el2-09.xml.exi",       "el2-09.xml");
    All().run("el2-10.xml.exi",       "el2-10.xml", false, true);

    All().run("022NooptB.exi",        "022.xml", false, true);
    // TODO: Fix exificient replacing & with &amp; and pruning CDATA
    // Also fix outputs being in the wrong order...
    All().run("044NooptB.exi",        "044.xml", false, true);
    //SetLogLevel(LogLevel::EXTRA);
    All().run("044rNooptB.exi",       "044r.xml", true);
    // [Fatal Error] :1:57: White space is required between the '%' and the
    // entity name in the parameter entity declaration.
    // [ERROR] org.xml.sax.SAXParseException; lineNumber: 1; columnNumber: 57;
    // White space is required between the '%' and the entity name in the parameter
    // entity declaration.class javax.xml.transform.TransformerException
    All().run("085NooptB.exi",        "085.xml", true);
    All().run("116NooptB.exi",        "116.xml", false, true);
  }
  ExificientFileData.RawCommands.push_back("");
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
#endif
  /*BitPacked - tests*/ {
    auto All = MAKE_TEST_RUNNER(AlignKind::BitPacked, "tests", All & ~LexicalValues);
    SetLogLevel(LogLevel::EXTRA);
    //All().run("o/xe/OiPcdip/021.exi", "s/xml/021.xml");
    //All().run("o/xe/OiPcdip/042.exi", "s/xml/042.xml");
  }

  return 0;

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

    errs() << "Running comparisons...\n";
    for (auto& [In, Out, P] : ExificientFileData.OldToNewMapping) {
      errs() << "  " << In << ": " << sys::path::filename(Out) << ' ';
      auto& InDoc = Mgr->getOptXMLDocument(In, errs()).expect("???");
      auto OutDoc = Mgr->getOptXMLDocument(Out, errs());
      if (!OutDoc) {
        WithColor(errs(), BRIGHT_YELLOW) << " [Failed to load]\n";
        continue;
      }

      SmallStr<0> ErrMsg;
      raw_svector_ostream OS(ErrMsg);
      XMLCompareOptions Opts {
        .OS = OS,
        .Preserve = P,
        .PreserveCDATA = XMLCompareOptions::CDATA_NONE,
        .ExificientCompatibility = true
      };

      const bool IsEqual = exi::compareXML(&InDoc, &*OutDoc, Opts);
      if (IsEqual)
        WithColor(errs(), BRIGHT_GREEN) << "[SUCCEEDED]\n";
      else {
        WithColor Save(errs(), BRIGHT_RED);
        Save << "[FAILED]\n";
        SmallVec<StrRef, 2> Errs;
        ErrMsg.str().split(Errs, '\n', -1, false);
        for (StrRef Err : Errs)
          Save << "    " << Err << '\n';
      }
    }
    errs() << '\n';
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
