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
struct ModCtx {
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
  
  ModCtx* Ctx = ptr_cast<ModCtx>(Raw);
  NameBuf Buf;
  Buf.loadNt_U(Entry->FullDllName);

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
  ModCtx Ctx { Patches, CB, true };
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

static LDRDataTableEntry* FindEntryForLoadedModule(void* Handle) {
  if UNLIKELY(!Handle)
    return nullptr;
  LDRDataTableEntry* Out = nullptr;
  i32 Status = LdrFindEntryForAddress(Handle, &Out);
  return (Status >= 0) ? Out : nullptr;
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
    const char* Name = Patch.FunctionName;
    if UNLIKELY(!Name)
      // Last element, shouldn't be found.
      break;
    if (Patch.TargetAddr != nullptr)
      // Address has already been found.
      continue;
    Patch.TargetAddr = Exports[Patch.TargetName];
    if (!Patch.TargetAddr)
      MiWarn("cannot resolve target %s.", Patch.TargetName);
    // Get for mode 2?
    if (Patch.TermName && !Patch.TermAddr) {
      Patch.TermAddr = Exports[Patch.TermName];
    }
  }
}

//////////////////////////////////////////////////////////////////////////
// Setup

static byte kInvalid = 1;

static bool CheckForCRT(StringRef Name);
static bool CheckForCRT(const char* Name) {
  return CheckForCRT(StringRef::New(Name));
}

static byte* GetDetouredAddress(DetourHandler Func) {
  return Func.getDetouredAddress();
}

static byte* CheckBasedFuncs(ExportHandler& Exp, StringRef Name) {
  if (!Name.ends_with("_base"_str))
    return nullptr;
  Name = Name.drop_back(sizeof("_base") - 1);

  byte* Out = &kInvalid;
  auto CheckBase = [&Exp, &Out, Name] (StringRef Base) -> bool {
    if (!Name.equals(Base))
      return false;
    byte* RealFunc
      = Exp.getExport<byte>(Base.data());
    Out = ::GetDetouredAddress(RealFunc);
    return true;
  };

  if (CheckBase("_realloc"_str))
    return Out;
  if (CheckBase("_msize"_str))
    return Out;
  if (CheckBase("_expand"_str))
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

  byte* Out = CheckBasedFuncs(
    Exp, StringRef::New(FuncName));
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
  if (auto* Mod = AnsiGetDllHandle(ImpModName)) {
    RVAHandler ModRVAs(Mod);
    Export = ModRVAs.getExport<byte>(FuncName);
    if (!Export)
      return nullptr;
    if (RelocOut)
      *RelocOut = Export;
    return Imp.findIATEntry(Export);
  }

  return nullptr;
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
      
      // IATEntry != nullptr
      Data.UsePatchedImports = true;
      Data.IATEntry = ptr_cast<void*>(IATEntry);

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
    
    // Resolved != nullptr
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

static const char* StrchrLast(const char* Haystack, const char Needle) {
  if UNLIKELY(!Haystack)
    return nullptr;
  const char* Out = nullptr;
  while (*Haystack != '\0') {
    if (*Haystack == Needle)
      Out = Haystack;
    ++Haystack;
  }
  return Out;
}

static bool CheckForCRT(StringRef Name) {
  // if (Name.starts_with_insensitive("msvcrt"_str))
  //   return true;
  return Name.starts_with_insensitive("ucrtbase"_str);
}

static bool IsPostWindows11Build() {
  // Checks for version 22H[N].
  return GetVersionTriple().Build >= 22000;
}

static bool CheckIfLoadedAndAttatched(
 const LDRDataTableEntry* Entry) {
  LDRDataTableEntry* EntryOut
    = FindEntryForLoadedModule(Entry->DllBase);
  if (EntryOut == nullptr)
    return false;
  if UNLIKELY(Entry != EntryOut) {
    MiTrace("entries do not match: %#p -> %#p", Entry, EntryOut);
    return true;
  }

  // Checks if ProcessAttachCalled is true.
  return (EntryOut->Flags & 0x80000) != 0;
}

template <usize Kind>
static void DumpLoadedModulesImpl(NameBuf& Buf) {
  using Ty = TLDRListEntry<Kind>;
  int Count = 0;

  for (auto* Link : Ty::Iterable()) {
    auto* Entry = Link->asDataTableEntry();
    Buf.loadNt_U(Entry->FullDllName);

    const char* Status;
    if ((Entry->Flags & 0x80000) == 0)
      Status = "un-initialized";
    else
      Status = "initialized";
    MiTrace("%d: %s, %s, base: 0x%x",
      Count, Buf.data(), Status, Count);
    ++Count;
  }
}

static void DumpLoadedModules(bool LoadOrInitOrder) {
  if (!::MIMALLOC_VERBOSE)
    // Exit early, can't see anything anyways.
    return;
  const bool IsLoad = (LoadOrInitOrder == 1);
  MiTrace("module %s order:",
    (IsLoad ? "load" : "initialization"));
  
  auto* Ntdll = AnsiGetDllHandle("ntdll.dll");
  if (Ntdll == nullptr)
    return;
  auto* Entry = FindEntryForLoadedModule(Ntdll);
  if (Entry == nullptr)
    return;
  
  NameBuf Buf;
  if (IsLoad)
    DumpLoadedModulesImpl<LDRListKind::loadOrder>(Buf);
  else
    DumpLoadedModulesImpl<LDRListKind::initOrder>(Buf);
}

static bool SetupPatching(
 StringRef Name,
 const LDRDataTableEntry* Entry,
 PPatchData Patches
) {
  MiTrace("module \"%s\"", Name.data());
  if (const char* Split = StrchrLast(Name.data(), '\\'))
    Name = {Split + 1, Name.end()};
  
  bool IsCRT = CheckForCRT(Name);
  bool IsShell = Name.starts_with_insensitive("shell32.dll"_str);

  if (IsCRT || (IsShell && IsPostWindows11Build())) {
    MiTrace("%s \"%s\"", (IsCRT ? "RESOLVING" : "resolving"), Name.data());
    RVAHandler RVAs(Entry->DllBase);
    ResolveFunctions(Name.data(), RVAs, Patches, !IsCRT);
    // bool IsLoaded = CheckIfLoadedAndAttatched(Entry);
    if (CheckIfLoadedAndAttatched(Entry)) {
      MiError("mimalloc-redirect.dll seems to be initialized after %s\n  "
              "(hint: try to link with the mimalloc library earlier "
              "on the command line?)",
        Name.data());
      DumpLoadedModules(true);
      DumpLoadedModules(false);
      MiError("\n");
      return false;
    }
  }

  return true;
}

HINSTANCE re::FindMimallocAndSetup(
  MutArrayRef<PerFuncPatchData> Patches,
  ArrayRef<const char*> Names,
  bool ForceRedirect
) {
  byte* Dll = nullptr;
  for (const char* Name : Names) {
    MiTrace("checking for target %s", Name);
    Dll = AnsiGetDllHandle(Name);
    if (Dll != nullptr) {
      // MiTrace("found %s!", Name);
      break;
    }
  }

  if (Dll == nullptr) {
    MiError("unable to find target module.");
    return nullptr;
  }

  UpdatePatchesFromMod(Dll, Patches);
  if (!CrawlLoadedModules(&SetupPatching, Patches)) {
    if (!ForceRedirect)
      return nullptr;
    MiWarn("there were errors during resolving but these are "
           "ignored (due to MIMALLOC_FORCE_REDIRECT=1).");
  }
  
  return reinterpret_cast<HINSTANCE>(Dll);
}

//======================================================================//
// Ordering
//======================================================================//

static LDRDataTableEntry* AnsiFindEntry(const char* Name) {
  WNameBuf UBuf;
  {
    if UNLIKELY(!Name || *Name == '\0')
      return nullptr;
    AnsiString Str;
    RtlInitAnsiString(&Str, Name);
    UBuf.loadNt_U(Str);
  }

  ArrayRef<wchar_t> UName = UBuf.buf();
  for (auto* Link : LoadOrderList::Iterable()) {
    LDRDataTableEntry* Entry
      = Link->asDataTableEntry();
    auto& Dll = Entry->BaseDllName;
    if (UName == Dll.buf())
      return Entry;
  }

  return nullptr;
}

static void LinkRemove(LoadOrderList* Link) {
  auto* Flink = Link->Flink;
  auto* Blink = Link->Blink;
  if (Flink)
    Flink->Blink = Blink;
  if (Blink)
    Blink->Flink = Flink;
}

static void LinkInsert(LoadOrderList* Prev, LoadOrderList* Next) {
  auto* Flink = Prev->Flink;
  Next->Flink = Flink;
  if (Flink)
    Flink->Blink = Next;
  Next->Blink = Prev;
  Prev->Flink = Next;
}

void re::PlaceDllAfterNtdllInLoadOrder(HINSTANCE Dll) {
  LDRDataTableEntry* MiTbl
    = FindEntryForLoadedModule(Dll);
  LDRDataTableEntry* NtTbl
    = AnsiFindEntry("ntdll.dll");
  if (!NtTbl || !MiTbl)
    return;
  if (NtTbl == MiTbl)
    return;
  
  auto* MiLink = MiTbl->inLoadOrder();
  auto* NtLink = NtTbl->inLoadOrder();
  // Check if already in the correct order.
  if (NtLink->Flink == MiLink)
    return;
  
  LinkRemove(MiLink);
  LinkInsert(NtLink, MiLink);
}
