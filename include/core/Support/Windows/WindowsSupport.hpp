//===- Support/Windows/WindowsSupport.hpp ----------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Relicensed under the Apache License, Version 2.0 (the "License");
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
//=== WARNING: Implementation here must contain only generic Win32 code that
//===          is guaranteed to work on *all* Win32 variants.
//===----------------------------------------------------------------===//

#pragma once

// mingw-w64 tends to define it as 0x0502 in its headers.
#undef _WIN32_WINNT

// Require at least Windows 7 API.
#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <Common/SmallVec.hpp>
#include <Common/StringExtras.hpp>
#include <Common/StrRef.hpp>
#include <Common/Twine.hpp>
#include <Support/Allocator.hpp>
#include <Support/Chrono.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/VersionTuple.hpp>
#include <string>
#include <system_error>
#include <windows.h>

// Must be included after windows.h
#include <wincrypt.h>

namespace exi {

/// Determines if the program is running on Windows 8 or newer. This
/// reimplements one of the helpers in the Windows 8.1 SDK, which are intended
/// to supercede raw calls to GetVersionEx. Old SDKs, Cygwin, and MinGW don't
/// yet have VersionHelpers.h, so we have our own helper.
bool RunningWindows8OrGreater();

/// Determines if the program is running on Windows 11 or Windows Server 2022.
bool RunningWindows11OrGreater();

/// Returns the Windows version as Major.Minor.0.BuildNumber. Uses
/// RtlGetVersion or GetVersionEx under the hood depending on what is available.
/// GetVersionEx is deprecated, but this API exposes the build number which can
/// be useful for working around certain kernel bugs.
exi::VersionTuple GetWindowsOSVersion();

bool MakeErrMsg(String *ErrMsg, const String &prefix);

// Include GetLastError() in a fatal error message.
[[noreturn]] inline void ReportLastErrorFatal(const char *Msg) {
  String ErrMsg;
  MakeErrMsg(&ErrMsg, Msg);
  exi::report_fatal_error(Twine(ErrMsg));
}

template <typename HandleTraits>
class ScopedHandle {
  typedef typename HandleTraits::handle_type handle_type;
  handle_type Handle;

  ScopedHandle(const ScopedHandle &other) = delete;
  void operator=(const ScopedHandle &other) = delete;
public:
  ScopedHandle()
    : Handle(HandleTraits::GetInvalid()) {}

  explicit ScopedHandle(handle_type h)
    : Handle(h) {}

  ~ScopedHandle() {
    if (HandleTraits::IsValid(Handle))
      HandleTraits::Close(Handle);
  }

  handle_type take() {
    handle_type t = Handle;
    Handle = HandleTraits::GetInvalid();
    return t;
  }

  ScopedHandle &operator=(handle_type h) {
    if (HandleTraits::IsValid(Handle))
      HandleTraits::Close(Handle);
    Handle = h;
    return *this;
  }

  // True if Handle is valid.
  explicit operator bool() const {
    return HandleTraits::IsValid(Handle) ? true : false;
  }

  operator handle_type() const {
    return Handle;
  }
};

struct CommonHandleTraits {
  typedef HANDLE handle_type;

  static handle_type GetInvalid() {
    return INVALID_HANDLE_VALUE;
  }

  static void Close(handle_type h) {
    ::CloseHandle(h);
  }

  static bool IsValid(handle_type h) {
    return h != GetInvalid();
  }
};

struct JobHandleTraits : CommonHandleTraits {
  static handle_type GetInvalid() {
    return NULL;
  }
};

struct CryptContextTraits : CommonHandleTraits {
  typedef HCRYPTPROV handle_type;

  static handle_type GetInvalid() {
    return 0;
  }

  static void Close(handle_type h) {
    ::CryptReleaseContext(h, 0);
  }

  static bool IsValid(handle_type h) {
    return h != GetInvalid();
  }
};

struct RegTraits : CommonHandleTraits {
  typedef HKEY handle_type;

  static handle_type GetInvalid() {
    return NULL;
  }

  static void Close(handle_type h) {
    ::RegCloseKey(h);
  }

  static bool IsValid(handle_type h) {
    return h != GetInvalid();
  }
};

struct FindHandleTraits : CommonHandleTraits {
  static void Close(handle_type h) {
    ::FindClose(h);
  }
};

struct FileHandleTraits : CommonHandleTraits {};

typedef ScopedHandle<CommonHandleTraits> ScopedCommonHandle;
typedef ScopedHandle<FileHandleTraits>   ScopedFileHandle;
typedef ScopedHandle<CryptContextTraits> ScopedCryptContext;
typedef ScopedHandle<RegTraits>          ScopedRegHandle;
typedef ScopedHandle<FindHandleTraits>   ScopedFindHandle;
typedef ScopedHandle<JobHandleTraits>    ScopedJobHandle;

template <class T>
class SmallVecImpl;

template <class T>
typename SmallVecImpl<T>::const_pointer
c_str(SmallVecImpl<T> &str) {
  str.push_back(0);
  str.pop_back();
  return str.data();
}

namespace sys {

inline std::chrono::nanoseconds toDuration(FILETIME Time) {
  ULARGE_INTEGER TimeInteger;
  TimeInteger.LowPart = Time.dwLowDateTime;
  TimeInteger.HighPart = Time.dwHighDateTime;

  // FILETIME's are # of 100 nanosecond ticks (1/10th of a microsecond)
  return std::chrono::nanoseconds(100 * TimeInteger.QuadPart);
}

inline TimePoint<> toTimePoint(FILETIME Time) {
  ULARGE_INTEGER TimeInteger;
  TimeInteger.LowPart = Time.dwLowDateTime;
  TimeInteger.HighPart = Time.dwHighDateTime;

  // Adjust for different epoch
  TimeInteger.QuadPart -= 11644473600ll * 10000000;

  // FILETIME's are # of 100 nanosecond ticks (1/10th of a microsecond)
  return TimePoint<>(std::chrono::nanoseconds(100 * TimeInteger.QuadPart));
}

inline FILETIME toFILETIME(TimePoint<> TP) {
  ULARGE_INTEGER TimeInteger;
  TimeInteger.QuadPart = TP.time_since_epoch().count() / 100;
  TimeInteger.QuadPart += 11644473600ll * 10000000;

  FILETIME Time;
  Time.dwLowDateTime = TimeInteger.LowPart;
  Time.dwHighDateTime = TimeInteger.HighPart;
  return Time;
}

namespace windows {

// Returns command line arguments. Unlike arguments given to main(),
// this function guarantees that the returned arguments are encoded in
// UTF-8 regardless of the current code page setting.
std::error_code GetCommandLineArguments(SmallVecImpl<const char *> &Args,
                                        BumpPtrAllocator &Alloc);

/// Convert UTF-8 path to a suitable UTF-16 path for use with the Win32 Unicode
/// File API.
std::error_code widenPath(const Twine &Path8, SmallVecImpl<wchar_t> &Path16,
                          size_t MaxPathLen = MAX_PATH);

} // namespace windows

} // namespace sys
} // namespace exi
