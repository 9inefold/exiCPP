//===- Support/ErrorHandle.cpp --------------------------------------===//
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

#include <Support/ErrorHandle.hpp>
#include <Common/String.hpp>
#include <Common/SmallStr.hpp>
#include <Common/Twine.hpp>
#include <Support/D/IO.hpp>
#include <Support/Errc.hpp>
#include <Support/Debug.hpp>
#include <Support/Format.hpp>
#include <Support/FmtBuffer.hpp>
#include <Support/Process.hpp>
#include <Support/Signals.hpp>
#include <Support/Stacktrace.hpp>
#include <Support/WindowsError.hpp>
#include <Support/raw_ostream.hpp>
#include <cstring>
#include <cstdlib>
#include <fmt/color.h>
#if EXI_EXCEPTIONS
# include <new>
# include <stdexcept>
#endif
#ifdef _WIN32
# define WIN32_NO_STATUS
# include <Support/Windows/WindowsSupport.hpp>
# undef WIN32_NO_STATUS
#endif

using namespace exi;

static const char* getAssertionMacro(H::AssertionKind Kind) {
  using enum H::AssertionKind;
  switch (Kind) {
   case ASK_Assert:
    return "exi_assert";
   case ASK_Assume:
    return "exi_assume";
   case ASK_Invariant:
    return "exi_invariant";
   case ASK_Unreachable:
    return "exi_unreachable";
   case ASK_Unimplemented:
    return "exi_unimplemented";
   case ASK_Todo:
    return "exi_todo";
   case ASK_Guardrail:
    return "exi_guardrail";
  }
  return "???";
}

static const char* getAssertionMessage(H::AssertionKind Kind) {
  using enum H::AssertionKind;
  switch (Kind) {
   case ASK_Assert:
    return "Assertion failed";
   case ASK_Assume:
    return "Assumption incorrect";
   case ASK_Invariant:
    return "Invariant broken";
   case ASK_Unreachable:
    return "Unreachable reached";
   case ASK_Unimplemented:
    return "Not implemented";
   case ASK_Todo:
    return "Not yet implemented";
   case ASK_Guardrail:
    return "Hit a guardrail";
  }
  return "??? failed";
}

static FmtBuffer::WriteState formatFatalError(FmtBuffer& Buf, StrRef Str) {
  const auto Out = Buf.format("exicpp error: {}\n", Str);
  if (Out != FmtBuffer::FullWrite)
    return Buf.setLast('\n');
  return Out;
}

[[noreturn]] static void reportFatalError(const Twine& Msg, bool GenCrashDiag) {
  StaticFmtBuffer<512> FullMsg;
  if (Msg.isSingleStrRef()) {
    // Trivial path, just grab the StrRef.
    formatFatalError(FullMsg, Msg.getSingleStrRef());
  } else {
    SmallStr<256> Buf;
    formatFatalError(FullMsg, Msg.toStrRef(Buf));
  }
  (void)::write(2, FullMsg.data(), FullMsg.size());

  // If we reached here, we are failing ungracefully. Run the interrupt handlers
  // to make sure any special cleanups get done, in particular that we remove
  // files registered with RemoveFileOnSignal.
  sys::RunInterruptHandlers();
  sys::Process::TrapIfDebugging();

  if (GenCrashDiag)
    std::abort();
  else
    std::exit(1);
}

[[noreturn]] EXI_ERROR_CC void
 exi::report_fatal_error(const char* Msg, bool GenCrashDiag) {
  reportFatalError(Twine(Msg), GenCrashDiag);
}

[[noreturn]] EXI_ERROR_CC void
 exi::report_fatal_error(StrRef Msg, bool GenCrashDiag) {
  reportFatalError(Twine(Msg), GenCrashDiag);
}

[[noreturn]] EXI_ERROR_CC void
 exi::report_fatal_error(const Twine& Msg, bool GenCrashDiag) {
  reportFatalError(Msg, GenCrashDiag);
}

[[noreturn]] EXI_COLD void exi::fatal_alloc_error(
 [[maybe_unused]] const char* Msg) EXI_NOEXCEPT {
#if EXI_EXCEPTIONS
  throw std::bad_alloc();
#else
  if (Msg == nullptr || Msg[0] == '\0')
    Msg = "Allocation failed.";
  
  const char* OOM = "ERROR: Out of memory.\n";
  (void)::write(2, OOM, std::strlen(OOM));
  (void)::write(2, Msg, std::strlen(Msg));
  (void)::write(2, "\n", 1);

  std::abort();
#endif
}

#if !EXI_EXCEPTIONS
void exi::install_out_of_memory_new_handler() {}
#else
// Causes crash on allocation failure. It is called prior to the handler set by
// 'install_bad_alloc_error_handler'.
static void out_of_memory_new_handler() {
  exi::fatal_alloc_error("Allocation failed");
}

// Installs new handler that causes crash on allocation failure.
// It is called by `InitDriver`.
void exi::install_out_of_memory_new_handler() {
  std::new_handler old = std::set_new_handler(&out_of_memory_new_handler);
  (void)old;
  exi_assert(old == nullptr || old == &out_of_memory_new_handler,
            "new-handler already installed");
}
#endif

// Used for clang.
#include "GetFileRelStart.hpp"

static consteval StrRef GetFileRelStartSrc() {
#if EXI_HAS_BUILTIN(__builtin_FILE)
  std::string_view File = __builtin_FILE();
#else
  std::string_view File = __FILE__;
#endif
  const usize Pos = File.find("exiCPP");
  if (Pos == File.npos)
    return ""_str;
  return StrRef(File.data(), Pos);
}

/// FIXME: Update separators on windows.
static bool ConsumeFileRelPfx(StrRef& S) {
  if (S.ends_with(".cpp"))
    return S.consume_front(GetFileRelStartSrc());
  else
    return S.consume_front(kHdrFilePfx);
}

static void AssertWithLocation(const char* File, const char* Func, unsigned Line) {
  SmallStr<256> Buf;
  raw_svector_ostream OS(Buf);

  using enum raw_ostream::Colors;
  OS.enable_colors(errs().colors_enabled());
  OS << RESET;

  if (Func && Func[0])
    OS << "In " << BRIGHT_GREEN
      << Func << ' ' << RESET;
  if (File && File[0]) {
    OS << "at " << BRIGHT_YELLOW;
    StrRef S(File);
    if (ConsumeFileRelPfx(S))
      OS << '@';
    OS << S << RESET;
    if (Line)
      OS << ':' << BLUE << Line << RESET;
  }
  
  if (!Buf.empty())
    fmt::print(stderr, "{}:\n  ", Buf.str());
}

static void AssertWithDetails(H::AssertionKind Kind, const char* Msg) {
  SmallStr<256> Buf;
  raw_svector_ostream OS(Buf);

  using enum raw_ostream::Colors;
  OS.enable_colors(errs().colors_enabled());
  OS << BRIGHT_RED
     << getAssertionMacro(Kind) << " - "
     << getAssertionMessage(Kind) << RESET;

  if (Msg && Msg[0])
    OS << ": " << YELLOW << Msg << RESET;
  
  fmt::println(stderr, "{}.", Buf.str());
}

[[noreturn]] EXI_ERROR_CC void exi::exi_assert_impl(
 H::AssertionKind Kind, const char* Msg,
 const char* File, const char* Func, unsigned Line
) {
  fmt::print(stderr, "\n");
  AssertWithLocation(File, Func, Line);
  AssertWithDetails(Kind, Msg);
#if EXI_DEBUG
  if (!sys::Process::IsDebugging()) {
    errs() << "\nStacktrace:\n";
    sys::PrintStackTrace(errs());
    // This avoids annoying signal handling.
    std::exit(1);
  }
#endif // EXI_DEBUG
  // Always abort in release.
  std::abort();
}

//======================================================================//
// Windows Only
//======================================================================//

#ifdef _WIN32

#include <ntstatus.h>
#include <winerror.h>

// This is equivalent to NtCurrentTeb()->LastStatusValue, but the public
// _TEB definition does not expose the LastStatusValue field directly.
// Avoid offsetting into this structure by calling RtlGetLastNtStatus
// from ntdll.dll.
//
// The return of this function will roughly match that of
// GetLastError, but this lower level API disambiguates some cases
// that GetLastError does not.
//
// For more information, see:
// https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/api/pebteb/teb/index.htm
// https://github.com/llvm/llvm-project/issues/89137
extern "C" NTSYSAPI NTSTATUS NTAPI RtlGetLastNtStatus(void);

// This function obtains the last error code and maps it. It may call
// RtlGetLastNtStatus, which is a lower level API that can return a
// more specific error code than GetLastError.
std::error_code exi::mapLastWindowsError() {
  unsigned EV = ::GetLastError();
  // The mapping of NTSTATUS to Win32 error loses some information; special
  // case the generic ERROR_ACCESS_DENIED code to check the underlying
  // NTSTATUS and potentially return a more accurate error code.
  if (EV == ERROR_ACCESS_DENIED) {
    exi::errc code = RtlGetLastNtStatus() == STATUS_DELETE_PENDING
                          ? errc::delete_pending
                          : errc::permission_denied;
    return make_error_code(code);
  }
  return mapWindowsError(EV);
}

// I'd rather not double the line count of the following.
#define MAP_ERR_TO_COND(x, y)                                                  \
  case x:                                                                      \
    return make_error_code(errc::y)

std::error_code exi::mapWindowsError(unsigned EV) {
  switch (EV) {
    MAP_ERR_TO_COND(ERROR_ACCESS_DENIED, permission_denied);
    MAP_ERR_TO_COND(ERROR_ALREADY_EXISTS, file_exists);
    MAP_ERR_TO_COND(ERROR_BAD_NETPATH, no_such_file_or_directory);
    MAP_ERR_TO_COND(ERROR_BAD_PATHNAME, no_such_file_or_directory);
    MAP_ERR_TO_COND(ERROR_BAD_UNIT, no_such_device);
    MAP_ERR_TO_COND(ERROR_BROKEN_PIPE, broken_pipe);
    MAP_ERR_TO_COND(ERROR_BUFFER_OVERFLOW, filename_too_long);
    MAP_ERR_TO_COND(ERROR_BUSY, device_or_resource_busy);
    MAP_ERR_TO_COND(ERROR_BUSY_DRIVE, device_or_resource_busy);
    MAP_ERR_TO_COND(ERROR_CANNOT_MAKE, permission_denied);
    MAP_ERR_TO_COND(ERROR_CANTOPEN, io_error);
    MAP_ERR_TO_COND(ERROR_CANTREAD, io_error);
    MAP_ERR_TO_COND(ERROR_CANTWRITE, io_error);
    MAP_ERR_TO_COND(ERROR_CURRENT_DIRECTORY, permission_denied);
    MAP_ERR_TO_COND(ERROR_DEV_NOT_EXIST, no_such_device);
    MAP_ERR_TO_COND(ERROR_DEVICE_IN_USE, device_or_resource_busy);
    MAP_ERR_TO_COND(ERROR_DIR_NOT_EMPTY, directory_not_empty);
    MAP_ERR_TO_COND(ERROR_DIRECTORY, invalid_argument);
    MAP_ERR_TO_COND(ERROR_DISK_FULL, no_space_on_device);
    MAP_ERR_TO_COND(ERROR_FILE_EXISTS, file_exists);
    MAP_ERR_TO_COND(ERROR_FILE_NOT_FOUND, no_such_file_or_directory);
    MAP_ERR_TO_COND(ERROR_HANDLE_DISK_FULL, no_space_on_device);
    MAP_ERR_TO_COND(ERROR_INVALID_ACCESS, permission_denied);
    MAP_ERR_TO_COND(ERROR_INVALID_DRIVE, no_such_device);
    MAP_ERR_TO_COND(ERROR_INVALID_FUNCTION, function_not_supported);
    MAP_ERR_TO_COND(ERROR_INVALID_HANDLE, invalid_argument);
    MAP_ERR_TO_COND(ERROR_INVALID_NAME, invalid_argument);
    MAP_ERR_TO_COND(ERROR_INVALID_PARAMETER, invalid_argument);
    MAP_ERR_TO_COND(ERROR_LOCK_VIOLATION, no_lock_available);
    MAP_ERR_TO_COND(ERROR_LOCKED, no_lock_available);
    MAP_ERR_TO_COND(ERROR_NEGATIVE_SEEK, invalid_argument);
    MAP_ERR_TO_COND(ERROR_NOACCESS, permission_denied);
    MAP_ERR_TO_COND(ERROR_NOT_ENOUGH_MEMORY, not_enough_memory);
    MAP_ERR_TO_COND(ERROR_NOT_READY, resource_unavailable_try_again);
    MAP_ERR_TO_COND(ERROR_NOT_SUPPORTED, not_supported);
    MAP_ERR_TO_COND(ERROR_OPEN_FAILED, io_error);
    MAP_ERR_TO_COND(ERROR_OPEN_FILES, device_or_resource_busy);
    MAP_ERR_TO_COND(ERROR_OUTOFMEMORY, not_enough_memory);
    MAP_ERR_TO_COND(ERROR_PATH_NOT_FOUND, no_such_file_or_directory);
    MAP_ERR_TO_COND(ERROR_READ_FAULT, io_error);
    MAP_ERR_TO_COND(ERROR_REPARSE_TAG_INVALID, invalid_argument);
    MAP_ERR_TO_COND(ERROR_RETRY, resource_unavailable_try_again);
    MAP_ERR_TO_COND(ERROR_SEEK, io_error);
    MAP_ERR_TO_COND(ERROR_SHARING_VIOLATION, permission_denied);
    MAP_ERR_TO_COND(ERROR_TOO_MANY_OPEN_FILES, too_many_files_open);
    MAP_ERR_TO_COND(ERROR_WRITE_FAULT, io_error);
    MAP_ERR_TO_COND(ERROR_WRITE_PROTECT, permission_denied);
    MAP_ERR_TO_COND(WSAEACCES, permission_denied);
    MAP_ERR_TO_COND(WSAEBADF, bad_file_descriptor);
    MAP_ERR_TO_COND(WSAEFAULT, bad_address);
    MAP_ERR_TO_COND(WSAEINTR, interrupted);
    MAP_ERR_TO_COND(WSAEINVAL, invalid_argument);
    MAP_ERR_TO_COND(WSAEMFILE, too_many_files_open);
    MAP_ERR_TO_COND(WSAENAMETOOLONG, filename_too_long);
  default:
    return std::error_code(EV, std::system_category());
  }
}

#endif
