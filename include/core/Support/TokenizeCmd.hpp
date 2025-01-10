//===- Support/TokenizeCmd.hpp ---------------------------------------===//
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

#pragma once

namespace exi {

template <typename> class SmallVecImpl;
class StrRef;
class StringSaver;

namespace cl {

/// Tokenizes a command line that can contain escapes and quotes.
//
/// The quoting rules match those used by GCC and other tools that use
/// libiberty's buildargv() or expandargv() utilities, and do not match bash.
/// They differ from buildargv() on treatment of backslashes that do not escape
/// a special character to make it possible to accept most Windows file paths.
///
/// \param [in] Source The string to be split on whitespace with quotes.
/// \param [in] Saver Delegates back to the caller for saving parsed strings.
/// \param [in] MarkEOLs true if tokenizing a response file and you want end of
/// lines and end of the response file to be marked with a nullptr string.
/// \param [out] NewArgv All parsed strings are appended to NewArgv.
void TokenizeGNUCommandLine(StrRef Source, StringSaver &Saver,
                            SmallVecImpl<const char *> &NewArgv,
                            bool MarkEOLs = false);

/// Tokenizes a string of Windows command line arguments, which may contain
/// quotes and escaped quotes.
///
/// See MSDN docs for CommandLineToArgvW for information on the quoting rules.
/// http://msdn.microsoft.com/en-us/library/windows/desktop/17w5ykft(v=vs.85).aspx
///
/// For handling a full Windows command line including the executable name at
/// the start, see TokenizeWindowsCommandLineFull below.
///
/// \param [in] Source The string to be split on whitespace with quotes.
/// \param [in] Saver Delegates back to the caller for saving parsed strings.
/// \param [in] MarkEOLs true if tokenizing a response file and you want end of
/// lines and end of the response file to be marked with a nullptr string.
/// \param [out] NewArgv All parsed strings are appended to NewArgv.
void TokenizeWindowsCommandLine(StrRef Source, StringSaver &Saver,
                                SmallVecImpl<const char *> &NewArgv,
                                bool MarkEOLs = false);

/// Tokenizes a Windows command line while attempting to avoid copies. If no
/// quoting or escaping was used, this produces substrings of the original
/// string. If a token requires unquoting, it will be allocated with the
/// StringSaver.
void TokenizeWindowsCommandLineNoCopy(StrRef Source, StringSaver &Saver,
                                      SmallVecImpl<StrRef> &NewArgv);

/// Tokenizes a Windows full command line, including command name at the start.
///
/// This uses the same syntax rules as TokenizeWindowsCommandLine for all but
/// the first token. But the first token is expected to be parsed as the
/// executable file name in the way CreateProcess would do it, rather than the
/// way the C library startup code would do it: CreateProcess does not consider
/// that \ is ever an escape character (because " is not a valid filename char,
/// hence there's never a need to escape it to be used literally).
///
/// Parameters are the same as for TokenizeWindowsCommandLine. In particular,
/// if you set MarkEOLs = true, then the first word of every line will be
/// parsed using the special rules for command names, making this function
/// suitable for parsing a file full of commands to execute.
void TokenizeWindowsCommandLineFull(StrRef Source, StringSaver &Saver,
                                    SmallVecImpl<const char *> &NewArgv,
                                    bool MarkEOLs = false);

/// String tokenization function type.  Should be compatible with either
/// Windows or Unix command line tokenizers.
using TokenizerCallback = void (*)(StrRef Source, StringSaver &Saver,
                                   SmallVecImpl<const char *> &NewArgv,
                                   bool MarkEOLs);

} // namespace cl
} // namespace exi
