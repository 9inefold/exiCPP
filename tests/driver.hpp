//===- driver.hpp ---------------------------------------------------===//
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

#pragma once

#include <Common/MMatch.hpp>
#include <Common/Result.hpp>
#include <Common/SmallStr.hpp>
#include <Common/StringSwitch.hpp>
#include <Support/Filesystem.hpp>
#include <Support/MemoryBuffer.hpp>
#include <Support/Path.hpp>
#include <Support/WithColor.hpp>
#include <Support/raw_ostream.hpp>
#include <exi/Basic/ExiOptions.hpp>
#include <exi/Basic/XMLContainer.hpp>

using namespace exi;

namespace driver {

using enum PreserveCDATAKind;

struct ExtraOptions {
  ExiOptions::PreserveOpts Preserve { .Prefixes = true };
  PreserveCDATAKind PreserveCDATA = CDATA_PRESERVE;
  AlignKind Align = AlignKind::BitPacked;
  Option<std::string> FileOut;
  bool MergeData = true;
  bool EscapeData = false;
};

inline void ParsePreserveOpts(ExiOptions::PreserveOpts& Opts, StrRef A) {
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
      //Opts.LexicalValues = true;
      break;
    case 'i':
    case 'I':
      Opts.PIs = true;
      break;
    case 'p':
    case 'P':
      // Always true:
      // Opts.Prefixes = true;
      break;
    default:
      //LOG_WARN("Unknown character '{}'", C);
      break;
    }
  }
}

inline Option<PreserveCDATAKind> ParseCDATAOpt(StrRef A) {
  if (A.empty())
    return std::nullopt;
  
  int V = unsigned(CDATA_PRESERVE);
  if (!A.consumeInteger(10, V)) {
    if (0 <= V && V <= 2)
      return PreserveCDATAKind(V);
    return std::nullopt;
  }

  return StringSwitch<Option<PreserveCDATAKind>>(A)
    .CasesLower("P", "PRESERVE", CDATA_PRESERVE)
    .CasesLower("E", "ESCAPE",   CDATA_ESCAPE)
    .CasesLower("N", "NONE",     CDATA_NONE)
    .Default(std::nullopt);
}

inline Option<AlignKind> ParseAlignOpt(StrRef A) {
  if (A.size() != 1)
    return std::nullopt;
  
  switch (A[0]) {
  case 'i':
  case 'I':
  case '0':
    return AlignKind::BitPacked;
  case 'y':
  case 'Y':
  case '1':
    return AlignKind::BytePacked;
  default:
    return std::nullopt;
  }
}

inline void DisableColors() {
  outs().enable_colors(false);
  errs().enable_colors(false);
#if EXI_DEBUG
  dbgs().enable_colors(false);
#endif
}

/// Returns command name if failed.
inline Result<bool, StrRef> ParseCommonArgs(StrRef Arg, ExtraOptions& Out) {
  if (Arg.consume_front("-O"))
    ParsePreserveOpts(Out.Preserve, Arg);
  else if (Arg.consume_front("-C")) {
    auto CDATA = ParseCDATAOpt(Arg);
    if (CDATA.has_value())
      Out.PreserveCDATA = *CDATA;
    else
      return Err("-C");
  } else if (Arg.consume_front("-A")) {
    auto Align = ParseAlignOpt(Arg);
    if (Align.has_value())
      Out.Align = *Align;
    else
      return Err("-A");
  } else if (Arg.consume_front("-T"))
    DisableColors();
  else
    return false;
  // Found command.
  return true;
}

//////////////////////////////////////////////////////////////

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

template <bool IsWritable = false>
inline auto LoadFileImpl(StrRef Path) {
  if constexpr (IsWritable)
    return WritableMemoryBuffer::getFileEx(Path, true);
  else
    return MemoryBuffer::getFile(Path);
}

template <bool IsWritable = false>
inline auto LoadFile(const Twine& Path) {
  SmallStr<80> Storage;
  Path.toVector(Storage);
  sys::fs::make_absolute(Storage);

  auto ErrOrBuf = LoadFileImpl<IsWritable>(Storage.str());
  if (!ErrOrBuf) {
    WithColor(errs(), raw_ostream::BRIGHT_RED)
      << "Error opening file: " << ErrOrBuf.getError().message() << "\n\n";
    exit(1);
  }

  return std::move(*ErrOrBuf);
}

inline Error WriteFile(StrRef File, StrRef Contents) {
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

inline bool ParseXMLFromBuf(XMLDocument& Doc, MemoryBuffer& MB) {
  static constexpr XMLParseOptions Default = {};
  if (Error E = exi::parseXMLFromBuffer(Doc, MB)) {
    logAllUnhandledErrors(std::move(E), errs());
    return true;
  }
  return false;
}

inline bool ParseXMLFromBuf(XMLDocument& Doc, WritableMemoryBuffer& MB,
                            Option<XMLParseOptions> Opts = std::nullopt) {
  static constexpr XMLParseOptions Default = {};
  if (Error E = exi::parseXMLFromBuffer(Doc, MB, Opts.value_or(Default))) {
    logAllUnhandledErrors(std::move(E), errs());
    return true;
  }
  return false;
}

} // namespace driver
