//===- Support/TokenizeCmd.cpp ---------------------------------------===//
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

#include <Support/TokenizeCmd.hpp>
#include <Common/FunctionRef.hpp>
#include <Common/SmallStr.hpp>
#include <Common/SmallVec.hpp>
#include <Common/StrRef.hpp>
#include <Common/StringExtras.hpp>
#include <Support/StringSaver.hpp>

using namespace exi;

static bool isWhitespace(char C) {
  return C == ' ' || C == '\t' || C == '\r' || C == '\n';
}

static bool isWhitespaceOrNull(char C) {
  return isWhitespace(C) || C == '\0';
}

static bool isQuote(char C) { return C == '\"' || C == '\''; }

// Windows treats whitespace, double quotes, and backslashes specially, except
// when parsing the first token of a full command line, in which case
// backslashes are not special.
static bool isWindowsSpecialChar(char C) {
  return isWhitespaceOrNull(C) || C == '\\' || C == '\"';
}
static bool isWindowsSpecialCharInCommandName(char C) {
  return isWhitespaceOrNull(C) || C == '\"';
}

/// Backslashes are interpreted in a rather complicated way in the Windows-style
/// command line, because backslashes are used both to separate path and to
/// escape double quote. This method consumes runs of backslashes as well as the
/// following double quote if it's escaped.
///
///  * If an even number of backslashes is followed by a double quote, one
///    backslash is output for every pair of backslashes, and the last double
///    quote remains unconsumed. The double quote will later be interpreted as
///    the start or end of a quoted string in the main loop outside of this
///    function.
///
///  * If an odd number of backslashes is followed by a double quote, one
///    backslash is output for every pair of backslashes, and a double quote is
///    output for the last pair of backslash-double quote. The double quote is
///    consumed in this case.
///
///  * Otherwise, backslashes are interpreted literally.
static usize parseBackslash(StrRef Src, usize I, SmallStr<128> &Token) {
  usize E = Src.size();
  int BackslashCount = 0;
  // Skip the backslashes.
  do {
    ++I;
    ++BackslashCount;
  } while (I != E && Src[I] == '\\');

  bool FollowedByDoubleQuote = (I != E && Src[I] == '"');
  if (FollowedByDoubleQuote) {
    Token.append(BackslashCount / 2, '\\');
    if (BackslashCount % 2 == 0)
      return I - 1;
    Token.push_back('"');
    return I;
  }
  Token.append(BackslashCount, '\\');
  return I - 1;
}

// Windows tokenization implementation. The implementation is designed to be
// inlined and specialized for the two user entry points.
static inline void tokenizeWindowsCommandLineImpl(
    StrRef Src, StringSaver &Saver, function_ref<void(StrRef)> AddToken,
    bool AlwaysCopy, function_ref<void()> MarkEOL, bool InitialCommandName) {
  SmallStr<128> Token;

  // Sometimes, this function will be handling a full command line including an
  // executable pathname at the start. In that situation, the initial pathname
  // needs different handling from the following arguments, because when
  // CreateProcess or cmd.exe scans the pathname, it doesn't treat \ as
  // escaping the quote character, whereas when libc scans the rest of the
  // command line, it does.
  bool CommandName = InitialCommandName;

  // Try to do as much work inside the state machine as possible.
  enum { INIT, UNQUOTED, QUOTED } State = INIT;

  for (usize I = 0, E = Src.size(); I < E; ++I) {
    switch (State) {
    case INIT: {
      assert(Token.empty() && "token should be empty in initial state");
      // Eat whitespace before a token.
      while (I < E && isWhitespaceOrNull(Src[I])) {
        if (Src[I] == '\n')
          MarkEOL();
        ++I;
      }
      // Stop if this was trailing whitespace.
      if (I >= E)
        break;
      usize Start = I;
      if (CommandName) {
        while (I < E && !isWindowsSpecialCharInCommandName(Src[I]))
          ++I;
      } else {
        while (I < E && !isWindowsSpecialChar(Src[I]))
          ++I;
      }
      StrRef NormalChars = Src.slice(Start, I);
      if (I >= E || isWhitespaceOrNull(Src[I])) {
        // No special characters: slice out the substring and start the next
        // token. Copy the string if the caller asks us to.
        AddToken(AlwaysCopy ? Saver.save(NormalChars) : NormalChars);
        if (I < E && Src[I] == '\n') {
          MarkEOL();
          CommandName = InitialCommandName;
        } else {
          CommandName = false;
        }
      } else if (Src[I] == '\"') {
        Token += NormalChars;
        State = QUOTED;
      } else if (Src[I] == '\\') {
        assert(!CommandName && "or else we'd have treated it as a normal char");
        Token += NormalChars;
        I = parseBackslash(Src, I, Token);
        State = UNQUOTED;
      } else {
        exi_unreachable("unexpected special character");
      }
      break;
    }

    case UNQUOTED:
      if (isWhitespaceOrNull(Src[I])) {
        // Whitespace means the end of the token. If we are in this state, the
        // token must have contained a special character, so we must copy the
        // token.
        AddToken(Saver.save(Token.str()));
        Token.clear();
        if (Src[I] == '\n') {
          CommandName = InitialCommandName;
          MarkEOL();
        } else {
          CommandName = false;
        }
        State = INIT;
      } else if (Src[I] == '\"') {
        State = QUOTED;
      } else if (Src[I] == '\\' && !CommandName) {
        I = parseBackslash(Src, I, Token);
      } else {
        Token.push_back(Src[I]);
      }
      break;

    case QUOTED:
      if (Src[I] == '\"') {
        if (I < (E - 1) && Src[I + 1] == '"') {
          // Consecutive double-quotes inside a quoted string implies one
          // double-quote.
          Token.push_back('"');
          ++I;
        } else {
          // Otherwise, end the quoted portion and return to the unquoted state.
          State = UNQUOTED;
        }
      } else if (Src[I] == '\\' && !CommandName) {
        I = parseBackslash(Src, I, Token);
      } else {
        Token.push_back(Src[I]);
      }
      break;
    }
  }

  if (State != INIT)
    AddToken(Saver.save(Token.str()));
}

//////////////////////////////////////////////////////////////////////////
// Implementation

void cl::TokenizeGNUCommandLine(StrRef Src, StringSaver &Saver,
                                SmallVecImpl<const char *> &NewArgv,
                                bool MarkEOLs) {
  SmallStr<128> Token;
  for (usize I = 0, E = Src.size(); I != E; ++I) {
    // Consume runs of whitespace.
    if (Token.empty()) {
      while (I != E && isWhitespace(Src[I])) {
        // Mark the end of lines in response files.
        if (MarkEOLs && Src[I] == '\n')
          NewArgv.push_back(nullptr);
        ++I;
      }
      if (I == E)
        break;
    }

    char C = Src[I];

    // Backslash escapes the next character.
    if (I + 1 < E && C == '\\') {
      ++I; // Skip the escape.
      Token.push_back(Src[I]);
      continue;
    }

    // Consume a quoted string.
    if (isQuote(C)) {
      ++I;
      while (I != E && Src[I] != C) {
        // Backslash escapes the next character.
        if (Src[I] == '\\' && I + 1 != E)
          ++I;
        Token.push_back(Src[I]);
        ++I;
      }
      if (I == E)
        break;
      continue;
    }

    // End the token if this is whitespace.
    if (isWhitespace(C)) {
      if (!Token.empty())
        NewArgv.push_back(Saver.save(Token.str()).data());
      // Mark the end of lines in response files.
      if (MarkEOLs && C == '\n')
        NewArgv.push_back(nullptr);
      Token.clear();
      continue;
    }

    // This is a normal character.  Append it.
    Token.push_back(C);
  }

  // Append the last token after hitting EOF with no whitespace.
  if (!Token.empty())
    NewArgv.push_back(Saver.save(Token.str()).data());
}

void cl::TokenizeWindowsCommandLine(StrRef Src, StringSaver &Saver,
                                    SmallVecImpl<const char *> &NewArgv,
                                    bool MarkEOLs) {
  auto AddToken = [&](StrRef Tok) { NewArgv.push_back(Tok.data()); };
  auto OnEOL = [&]() {
    if (MarkEOLs)
      NewArgv.push_back(nullptr);
  };
  tokenizeWindowsCommandLineImpl(Src, Saver, AddToken,
                                 /*AlwaysCopy=*/true, OnEOL, false);
}

void cl::TokenizeWindowsCommandLineNoCopy(StrRef Src, StringSaver &Saver,
                                          SmallVecImpl<StrRef> &NewArgv) {
  auto AddToken = [&](StrRef Tok) { NewArgv.push_back(Tok); };
  auto OnEOL = []() {};
  tokenizeWindowsCommandLineImpl(Src, Saver, AddToken, /*AlwaysCopy=*/false,
                                 OnEOL, false);
}

void cl::TokenizeWindowsCommandLineFull(StrRef Src, StringSaver &Saver,
                                        SmallVecImpl<const char *> &NewArgv,
                                        bool MarkEOLs) {
  auto AddToken = [&](StrRef Tok) { NewArgv.push_back(Tok.data()); };
  auto OnEOL = [&]() {
    if (MarkEOLs)
      NewArgv.push_back(nullptr);
  };
  tokenizeWindowsCommandLineImpl(Src, Saver, AddToken,
                                 /*AlwaysCopy=*/true, OnEOL, true);
}
