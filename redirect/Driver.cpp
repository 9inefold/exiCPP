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

#include <ArrayRef.hpp>
#include <Buf.hpp>
#include <Detours.hpp>
#include <Env.hpp>
#include <Globals.hpp>
#include <Logging.hpp>
#include <Mem.hpp>
#include <RVA.hpp>
#include <Strings.hpp>
#include <WinAPI.hpp>
#include <Version.hpp>

using namespace re;

static bool PrioritizeLoadOrder = true;
static bool ForceRedirect = false;

static bool IsTrueEnvBuf(NameBuf& Buf) {
  if (Buf.empty())
    return false;
  return FindEnvInParamList(Buf, true);
}

static bool IsRedirectDisabled(NameBuf& Buf) {
  bool DisableRedirectOrOverride
    = FindEnvironmentVariable("MIMALLOC_DISABLE_REDIRECT", Buf);
  if (!DisableRedirectOrOverride) {
    DisableRedirectOrOverride
      = FindEnvironmentVariable("MIMALLOC_DISABLE_OVERRIDE", Buf);
  }

  bool IsDisabled = false;
  if (DisableRedirectOrOverride) {
    if (Buf.empty())
      IsDisabled = true;
    else if (!FindEnvInParamList(Buf, true))
      IsDisabled = true;
  }

  if (not IsDisabled) {
    bool Found = FindEnvironmentVariable("MIMALLOC_ENABLE_REDIRECT", Buf);
    if (not Found || !FindEnvInParamList(Buf, false))
      IsDisabled = false;
    else
      IsDisabled = true;
  }

  return IsDisabled;
}

/// Initializes several global variables.
static void InitGlobals(NameBuf& Buf) {
  bool Found = FindEnvironmentVariable("MIMALLOC_VERBOSE", Buf);
  if (!Found || Buf.Data[0] != '3')
    MIMALLOC_VERBOSE = false;

  Found = FindEnvironmentVariable("MIMALLOC_PRIORITIZE_LOAD_ORDER", Buf);
  if (!Found || !IsTrueEnvBuf(Buf))
    ::PrioritizeLoadOrder = false;
  
  Found = FindEnvironmentVariable("MIMALLOC_PATCH_IMPORTS", Buf);
  if (!Found || !IsTrueEnvBuf(Buf))
    MIMALLOC_PATCH_IMPORTS = false;

  Found = FindEnvironmentVariable("MIMALLOC_FORCE_REDIRECT", Buf);
  if (Found && FindEnvInParamList(Buf, false))
    ::ForceRedirect = false;
}

static bool Driver(HINSTANCE Dll) {
  NameBuf Buf {};
  if (IsRedirectDisabled(Buf))
    return true;
  InitGlobals(Buf);

  MiTrace("build: %s", __DATE__);
  auto [Major, Minor, Build] = GetVersionTriple();
  MiTrace("windows version: %u.%u.%u", Major, Minor, Build);

  const char* DllNames[] {
    "mimalloc.dll",
    "mimalloc-override.dll",
    "mimalloc-secure.dll",
    "mimalloc-secure-debug.dll",
    "mimalloc-debug.dll",
    "mimalloc-release.dll"
  };

  HINSTANCE MiDll = FindMimallocAndSetup(
    GetPatches(), DllNames, ::ForceRedirect);
  if (MiDll == nullptr)
    return true;
  
  ::mi_redirect_entry = RVAHandler(MiDll)
    .getExport<_mi_redirect_entry>("_mi_redirect_entry");
  if (::mi_redirect_entry && ::PrioritizeLoadOrder)
    PlaceDllAfterNtdllInLoadOrder(MiDll);
  
  PatchResult Result
    = HandlePatching(PM_PATCH, GetPatches());
  if (Result == PR_SUCCESS || ::ForceRedirect) {
    if (::ForceRedirect)
      MiWarn("there were errors during patching but these are ignored "
             "(due to MIMALLOC_FORCE_REDIRECT=1).");
    NO_PATCH_ERRORS = true;
  } else if (Result == PR_FAILED) {
    MiWarn("redirection patching failed");
  } else /*Result == PR_PARTIAL*/ {
    MiWarn("redirection failed with partially applied patches -- "
           "aborting dll loading.");
    return false;
  }

  return true;
}

static bool DriverMain(HINSTANCE Dll, DWORD Reason) {
  bool Ret = Driver(Dll);
  if (NO_PATCH_ERRORS) {
    MiTrace("standard malloc is redirected (v1.1c)");
  } else {
    MiWarn("standard malloc is _not_ redirected! -- "
           "using regular malloc/free. (v1.1c)");
  }

  if (NO_PATCH_ERRORS && mi_redirect_entry != nullptr) {
    mi_redirect_entry(Reason);
  }
  return Ret;
}

extern "C" BOOL WINAPI DllMainCRTStartup(HINSTANCE Dll, DWORD Reason, PVOID) {
  bool Result = true;
  if (Reason == DLL_PROCESS_ATTACH)
    Result = DriverMain(Dll, Reason);
  // TODO: Invoke mi_redirect_entry
  return Result;
}
