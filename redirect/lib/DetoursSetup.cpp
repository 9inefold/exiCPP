//===- DetoursSetup.cpp ---------------------------------------------===//
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
///
/// \file
/// This file implements the detour setup.
///
//===----------------------------------------------------------------===//

#include <Detours.hpp>
#include <Buf.hpp>
#include <Globals.hpp>
#include <Logging.hpp>
#include <RVA.hpp>
#include <Strings.hpp>
#include <Version.hpp>
#include "NtImports.hpp"

using namespace re;

using StringRef = ArrayRef<char>;
using PPatchData = MutArrayRef<PerFuncPatchData>;
using ModuleCrawlerFunc = bool(
  StringRef Name,
  const LDRDataTableEntry* Entry,
  PPatchData Patches);

namespace {
struct Context {
  PPatchData Patches;
  ModuleCrawlerFunc* Callback;
  bool Result = true;
};
} // namespace `anonymous`

static void CrawlerProc(LDRDataTableEntry* Entry, void* Raw, bool*) {
  HANDLE Mod = nullptr;
  i32 Status = LdrGetDllHandle(
    nullptr, nullptr, &Entry->FullDllName, &Mod);
  if (Status < 0)
    return;
  
  Context* Ctx = ptr_cast<Context>(Raw);
  NameBuf Buf;
  {
    AnsiString Str;
    Buf.setNt(Str);
    RtlUnicodeStringToAnsiString(
      &Str, &Entry->FullDllName, false);
    Buf.loadNt(Str);
  }

  if (Mod != Entry->DllBase) {
    MiTrace("entries for \"%s\" do not match: %#p -> %#p",
      Buf.data(), Entry->DllBase, Mod);
    return;
  }

  re_assert(Ctx && Ctx->Callback);
  StringRef Str {Buf.data(), Buf.size()};
  bool Res = (*Ctx->Callback)(Str, Entry, Ctx->Patches);
  if (!Res)
    Ctx->Result = false;
}

static bool CrawlLoadedModules(ModuleCrawlerFunc* CB, PPatchData Patches) {
  Context Ctx { Patches, CB, true };
  i32 Status = LdrEnumerateLoadedModules(nullptr, &CrawlerProc, &Ctx);
  if (Status < 0 || !Ctx.Result) {
    MiWarn("module crawler status: %#x", u32(Status));
    return false;
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////
// Preloading

static byte* AnsiGetDllHandle(const char* Filename) noexcept {
  if UNLIKELY(!Filename)
    return nullptr;
  
  AnsiString Str;
  RtlInitAnsiString(&Str, Filename);

  WNameBuf UBuf;
  UnicodeString UStr;
  UBuf.setNt(UStr);
  RtlAnsiStringToUnicodeString(&UStr, &Str, false);

  HANDLE Out = nullptr;
  i32 Status = LdrGetDllHandle(nullptr, nullptr, &UStr, &Out);

  if (Status < 0)
    return nullptr;
  return ptr_cast<byte>(Out);
}

ALWAYS_INLINE static bool VerifySize(RVAHandler& RVAs) {
#if EXI_DEBUG
  for (auto* Link : LoadOrderList::Iterable()) {
    auto* Tbl = Link->asDataTableEntry();
    if (Tbl->DllBase != RVAs.base())
      continue;
    if (Tbl->SizeOfImage == RVAs.size())
      return true;
    MiWarn("incorrect image size: %#p -> %#p",
      Tbl->SizeOfImage, RVAs.size());
    break;
  }
#endif
  return !EXI_DEBUG;
}

static void UpdatePatchesFromMod(void* Mod, PPatchData Patches) {
  RVAHandler RVAs(Mod);
  VerifySize(RVAs);

  ExportHandler Exports = RVAs.exports();
  for (PerFuncPatchData& Patch : Patches) {
    if (Patch.TargetAddr != nullptr)
      continue;
    const char* Name = Patch.FunctionName;
    Patch.TargetAddr = Exports[Patch.TargetName];
    if (!Patch.TargetAddr && !Patch.ModuleName)
      MiWarn("cannot resolve target %s.", Name);
    // Get for mode 2?
    if (Patch.TermName && !Patch.TermAddr) {
      Patch.TermAddr = Exports[Patch.TermName];
    }
  }
}

//////////////////////////////////////////////////////////////////////////
// Setup

const byte kInvalid = 0;

static bool CheckForCRT(StringRef Name);
static bool CheckForCRT(const char* Name) {
  StringRef Str(Name, Strlen(Name));
  return CheckForCRT(Str);
}

static byte* GetDetouredAddress(DetourHandler Func) {
  return Func.getDetouredAddress();
}

static byte* CheckBasedFuncs(ExportHandler& Exp, StringRef Name) {
  if (!Name.ends_with("_base"))
    return nullptr;
  Name = Name.drop_back(sizeof("_base") - 1);

  byte* Out = const_cast<byte*>(&kInvalid);
  auto CheckBase = [&Exp, &Out, Name] (StringRef Base) -> bool {
    if (!Name.equals(Base))
      return false;
    byte* RealFunc
      = Exp.getExport<byte>(Base.data());
    Out = ::GetDetouredAddress(RealFunc);
    return true;
  };

  if (CheckBase("_realloc"))
    return Out;
  if (CheckBase("_msize"))
    return Out;
  if (CheckBase("_expand"))
    return Out;
  return Out;
}

static byte* ResolveExport(
 ExportHandler& Exp, const char* ModName,
 const char* FuncName) {
  if UNLIKELY(!FuncName)
    return nullptr;
  if (byte* FuncAddr =
      Exp.getExport<byte>(FuncName)) {
    return FuncAddr;
  }

  StringRef Name(FuncName, Strlen(FuncName));
  byte* Out = CheckBasedFuncs(Exp, Name);

  if (Out == &kInvalid)
    // No match.
    return nullptr;
  
  if (!Out && !CheckForCRT(ModName)) {
    MiWarn("unable to resolve \"%s!%s\" -- "
           "enabling MIMALLOC_PATCH_IMPORTS to prevent allocation errors.",
      ModName, FuncName);
    MIMALLOC_PATCH_IMPORTS = true;
  }
  return Out;
}

static byte** ResolveImport(
 ImportHandler& Imp, const char* ImpModName,
 const char* FuncName, byte** RelocOut = nullptr) {
  if (RelocOut)
    *RelocOut = nullptr;

  byte* Export = nullptr;
  if (auto Mod = AnsiGetDllHandle(ImpModName)) {
    RVAHandler ModRVAs(Mod);
    Export = ModRVAs.getExport<byte>(FuncName);
    if (RelocOut && Export)
      *RelocOut = Export;
  }

  return Imp.findIATEntry(Export);
}

static byte* GetCodeSegment(RVAHandler& RVAs, usize* FileAddr = nullptr) {
  auto Nt = RVAs.getNt();
  re_assert(Nt != nullptr);

  if (FileAddr)
    *FileAddr = 0;

  constexpr u32 ExpectedRVAs = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
  if UNLIKELY(Nt->OptionalHeader.NumberOfRvaAndSizes != ExpectedRVAs) {
    usize DirectoryCount = Nt->OptionalHeader.NumberOfRvaAndSizes;
    MiTrace("Expected %u entries, got %zu", ExpectedRVAs, DirectoryCount);
    return nullptr;
  }

  usize SecOff  = RVAs.getNtOffset() + 0x108;
  usize SecLen  = Nt->FileHeader.NumberOfSections;
  auto Sections = RVAs.getArr<IMAGE_SECTION_HEADER>(SecOff, SecLen);

  for (IMAGE_SECTION_HEADER& Sec : Sections) {
    auto* Name = ptr_cast<const char>(Sec.Name);
    if (not Strequal(Name, ".text", 5))
      continue;
    // Found `.text`...
    if (FileAddr)
      *FileAddr = Sec.Misc.PhysicalAddress;
    return RVAs.get<byte>(Sec.VirtualAddress);
  }

  return nullptr;
}

static OptInt<int> FindUnusedPatch(PerFuncPatchData& Patch) {
  for (int Ix = 0; Ix < kPatchDataCount; ++Ix) {
    if (Patch.Patches[Ix].FunctionData == nullptr)
      return Ix;
  }
  return re::nullopt;
}

static void ResolveFunctions(
 const char* ModName,
 RVAHandler& RVAs,
 PPatchData Patches,
 bool ForceRedirect
) {
  usize PhysicalAddr = 0;
  byte* CodeSeg = GetCodeSegment(RVAs, &PhysicalAddr);
  MiTrace("module: %s %#p: code start 0x%#p, size: 0x%zx",
    ModName, RVAs.base(), CodeSeg, PhysicalAddr);
  
  ExportHandler Exp = RVAs.exports();
  ImportHandler Imp = RVAs.imports();
  for (PerFuncPatchData& Patch : Patches) {
    auto Ix = FindUnusedPatch(Patch);
    if UNLIKELY(!Ix)
      continue;
    
    // Exit early if we want redirects but shouldn't import.
    if (ForceRedirect && !Patch.ModuleName)
      continue;

    auto& Data = Patch.Patches[*Ix];
    if (Patch.ModuleName &&
        (MIMALLOC_PATCH_IMPORTS || ForceRedirect)) {
      // If we get here, we check for imports in the specified module.
      byte** IATEntry =
        ResolveImport(Imp, Patch.ModuleName, Patch.FunctionName);
      if (!IATEntry)
        continue;
      if (byte** FnRVA = Patch.FunctionRVA)
        *FnRVA = *IATEntry;
      
      Data.UsePatchedImports = true;
      Data.IATEntry = IATEntry;

      MiTrace("resolve import \"%s!%s\" in %s at %#p to %#p (%i)",
        Patch.ModuleName, Patch.FunctionName,
        ModName, IATEntry, Patch.TargetAddr, *Ix
      );
      continue;
    }

    // Past here we just search through exports.
    byte* Resolved = ResolveExport(
      Exp, ModName, Patch.FunctionName);
    if (!Resolved)
      continue;
    Data.UsePatchedImports = false;
    Data.FunctionData = Resolved;
    Data.StoreFunc = CodeSeg;
    Data.StoreSize = PhysicalAddr;

    MiTrace("resolve \"%s\" at %s!%p to mimalloc!%p (%i)",
      Patch.FunctionName, ModName,
      Resolved, Patch.TargetAddr, *Ix
    );
  }
}

static bool CheckForCRT(StringRef Name) {
  if (Name.starts_with_insensitive("ucrtbase"))
    return true;
  return Name.starts_with_insensitive("msvcrt");
}

static bool IsPostWindows11Build() {
  // Checks for version 22H[N].
  return GetVersionTriple().Build >= 22000;
}

static bool CheckIfLoadedAndAttatched(
 const LDRDataTableEntry* Entry) {
  // LdrFindEntryForAddress()
  for (auto* Link : InitOrderList::Iterable()) {
    auto* Tbl = Link->asDataTableEntry();
    if (Tbl->DllBase == Entry->DllBase) {
      return (Tbl->Flags & 0x80000) != 0;
    }
  }

  return false;
}

static bool SetupPatching(
 StringRef Name,
 const LDRDataTableEntry* Entry,
 PPatchData Patches
) {
  if (const char* Split = Strchr(Name.data(), '\\'))
    Name = {Split + 1, Name.end()};
  MiTrace("module \"%s\"", Name.data());

  bool IsCRT = CheckForCRT(Name);
  bool IsShell = Name.starts_with_insensitive("shell32.dll");

  if (IsCRT || (!IsShell && IsPostWindows11Build())) {
    MiTrace("resolving \"%s\"", Name.data());
    RVAHandler RVAs(Entry->DllBase);
    ResolveFunctions(Name.data(), RVAs, Patches, !IsCRT);
    bool IsLoaded = CheckIfLoadedAndAttatched(Entry);
  }

  return true;
}

void* re::FindMimallocAndSetup(
  MutArrayRef<PerFuncPatchData> Patches,
  ArrayRef<const char*> Names,
  bool ForceRedirect
) {
  byte* Dll = nullptr;
  for (const char* Name : Names) {
    MiTrace("checking for target %s", Name);
    Dll = AnsiGetDllHandle(Name);
    if (Dll != nullptr) {
      MiTrace("found %s!", Name);
      break;
    }
  }

  if (Dll == nullptr) {
    MiError("unable to find target module.");
    return nullptr;
  }

  UpdatePatchesFromMod(Dll, Patches);
  bool DidSetup = CrawlLoadedModules(&SetupPatching, Patches);
  // "there were errors during resolving but these are ignored (due to MIMALLOC_FORCE_REDIRECT=1)."

  return Dll;
}
