//===- Support/Error.cpp ---------------------------------------------===//
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

#include <Support/Error.hpp>
#include <Common/SmallVec.hpp>
#include <Common/StringExtras.hpp>
#include <Common/Twine.hpp>
#include <Support/ErrorHandle.hpp>
#include <system_error>

using namespace exi;

namespace {
  enum class ErrorErrorCode : int {
    MultipleErrors = 1,
    FileError,
    InconvertibleError
  };

  // FIXME: This class is only here to support the transition to llvm::Error. It
  // will be removed once this transition is complete. Clients should prefer to
  // deal with the Error value directly, rather than converting to error_code.
  class ErrorErrorCategory : public std::error_category {
  public:
    const char *name() const noexcept override { return "Error"; }

    String message(int condition) const override {
      switch (static_cast<ErrorErrorCode>(condition)) {
      case ErrorErrorCode::MultipleErrors:
        return "Multiple errors";
      case ErrorErrorCode::InconvertibleError:
        return "Inconvertible error value. An error has occurred that could "
               "not be converted to a known std::error_code. Please file a "
               "bug.";
      case ErrorErrorCode::FileError:
          return "A file error occurred.";
      }
      exi_unreachable("Unhandled error code");
    }
  };
} // namespace `anonymous`

ErrorErrorCategory &getErrorErrorCat() {
  static ErrorErrorCategory ErrorErrorCat;
  return ErrorErrorCat;
}

namespace exi {

void ErrorInfoBase::anchor() {}
char ErrorInfoBase::ID = 0;
char ErrorList::ID = 0;
void ECError::anchor() {}
char ECError::ID = 0;
char StringError::ID = 0;
char FileError::ID = 0;

void logAllUnhandledErrors(Error E, raw_ostream &OS, Twine ErrorBanner) {
  if (!E)
    return;
  OS << ErrorBanner;
  handleAllErrors(std::move(E), [&](const ErrorInfoBase &EI) {
    EI.log(OS);
    OS << "\n";
  });
}

/// Write all error messages (if any) in E to a string. The newline character
/// is used to separate error messages.
String toString(Error E) {
  SmallVec<String, 2> Errors;
  handleAllErrors(std::move(E), [&Errors](const ErrorInfoBase &EI) {
    Errors.push_back(EI.message());
  });
  return join(Errors.begin(), Errors.end(), "\n");
}

String toStringWithoutConsuming(const Error &E) {
  SmallVec<String, 2> Errors;
  visitErrors(E, [&Errors](const ErrorInfoBase &EI) {
    Errors.push_back(EI.message());
  });
  return join(Errors.begin(), Errors.end(), "\n");
}

std::error_code ErrorList::convertToErrorCode() const {
  return std::error_code(static_cast<int>(ErrorErrorCode::MultipleErrors),
                         getErrorErrorCat());
}

std::error_code inconvertibleErrorCode() {
  return std::error_code(static_cast<int>(ErrorErrorCode::InconvertibleError),
                         getErrorErrorCat());
}

std::error_code FileError::convertToErrorCode() const {
  std::error_code NestedEC = Err->convertToErrorCode();
  if (NestedEC == inconvertibleErrorCode())
    return std::error_code(static_cast<int>(ErrorErrorCode::FileError),
                           getErrorErrorCat());
  return NestedEC;
}

Error errorCodeToError(std::error_code EC) {
  if (!EC)
    return Error::success();
  return Error(std::make_unique<ECError>(ECError(EC)));
}

std::error_code errorToErrorCode(Error Err) {
  std::error_code EC;
  handleAllErrors(std::move(Err), [&](const ErrorInfoBase &EI) {
    EC = EI.convertToErrorCode();
  });
  if (EC == inconvertibleErrorCode())
    report_fatal_error(Twine(EC.message()));
  return EC;
}

#if EXI_INVARIANTS
void Error::fatalUncheckedError() const {
# if EXI_HAS_DBG_IMPL
  dbgs() << "Program aborted due to an unhandled Error:\n";
  if (getPtr()) {
    getPtr()->log(dbgs());
    dbgs() << "\n";
  } else {
    dbgs() << "Error value was Success. (Note: Success values must still be "
              "checked prior to being destroyed).\n";
  }
# endif
  abort();
}
#endif

StringError::StringError(std::error_code EC, const Twine &S)
    : Msg(S.str()), EC(EC) {}

StringError::StringError(const Twine &S, std::error_code EC)
    : Msg(S.str()), EC(EC), PrintMsgOnly(true) {}

StringError::StringError(String &&S, std::error_code EC, bool PrintMsgOnly)
    : Msg(S), EC(EC), PrintMsgOnly(PrintMsgOnly) {}

void StringError::log(raw_ostream &OS) const {
  if (PrintMsgOnly) {
    OS << Msg;
  } else {
    OS << EC.message();
    if (!Msg.empty())
      OS << (" " + Msg);
  }
}

std::error_code StringError::convertToErrorCode() const {
  return EC;
}

Error createStringError(String &&Msg, std::error_code EC) {
  return make_error<StringError>(Msg, EC);
}

void report_fatal_error(Error Err, bool GenCrashDiag) {
  exi_assert(Err, "report_fatal_error called with success value");
  String ErrMsg;
  {
    raw_string_ostream ErrStream(ErrMsg);
    logAllUnhandledErrors(std::move(Err), ErrStream);
  }
  report_fatal_error(Twine(ErrMsg), GenCrashDiag);
}

} // namespace exi
