//===- Common/StringExtras.cpp --------------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024-2026 Ninefold
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


#include <Common/StringExtras.hpp>
#include <Common/SmallVec.hpp>
#include <Common/function_ref.hpp>
#include <Support/raw_ostream.hpp>
#include <Common/StrRef-inl.hpp>
#include <cctype>

using namespace exi;

ArrayRef<char> exi::toCommonStringBuf(StrRef String) {
  return ArrayRef<char>(String.begin(), String.end());
}

MutArrayRef<char> exi::toCommonStringBuf(String& String) {
  return exi::toCommonStringBuf(String.data(), String.size());
}

ArrayRef<char> exi::toCommonStringBuf(const String& String) {
  return exi::toCommonStringBuf(String.data(), String.size());
}

/// StrInStrNoCase - Portable version of strcasestr.  Locates the first
/// occurrence of string 's1' in string 's2', ignoring case.  Returns
/// the offset of s2 in s1 or npos if s2 cannot be found.
StrRef::size_type exi::StrInStrNoCase(StrRef s1, StrRef s2) {
  size_t N = s2.size(), M = s1.size();
  if (N > M)
    return StrRef::npos;
  for (size_t i = 0, e = M - N + 1; i != e; ++i)
    if (s1.substr(i, N).equals_insensitive(s2))
      return i;
  return StrRef::npos;
}

/// getToken - This function extracts one token from source, ignoring any
/// leading characters that appear in the Delimiters string, and ending the
/// token at any of the characters that appear in the Delimiters string. If
/// there are no tokens in the source string, an empty string is returned.
/// The function returns a pair containing the extracted token and the
/// remaining tail string.
std::pair<StrRef, StrRef> exi::getToken(StrRef Source, StrRef Delimiters) {
  auto [Start, End] = Source.find_token(Delimiters);
  return std::make_pair(Source.slice(Start, End), Source.substr(End));
}

/// getToken - This function extracts one token from source, ignoring any
/// leading characters that appear in the Delimiters filter, and ending the
/// token at any of the characters that appear in the Delimiters filter. If
/// there are no tokens in the source string, an empty string is returned.
/// The function returns a pair containing the extracted token and the
/// remaining tail string.
std::pair<StrRef, StrRef> exi::getToken(StrRef Source,
                                        const StrRef::filter_t& F) {
  auto [Start, End] = Source.find_token(F);
  return std::make_pair(Source.slice(Start, End), Source.substr(End));
}

/// SplitString - Split up the specified string according to the specified
/// delimiters, appending the result fragments to the output list.
void exi::SplitString(StrRef Source,
                      SmallVecImpl<StrRef> &OutFragments,
                      const StrRef::filter_t &Filter) {
  std::pair<StrRef, StrRef> Str = getToken(Source, Filter);
  while (!Str.first.empty()) {
    OutFragments.push_back(Str.first);
    Str = getToken(Str.second, Filter);
  }
}

/// SplitString - Split up the specified string according to the specified
/// delimiters, appending the result fragments to the output list.
void exi::SplitString(StrRef Source,
                      SmallVecImpl<StrRef> &OutFragments,
                      StrRef Delimiters) {
  StrRef::filter_t CharBits {};
  for (char C : Delimiters)
    CharBits.set((unsigned char)C);
  SplitString(Source, OutFragments, CharBits);
}

raw_ostream &exi::printEscapedString(StrRef Name, raw_ostream &Out) {
  for (unsigned char C : Name) {
    if (C == '\\')
      Out << '\\' << C;
    else if (isPrint(C) && C != '"')
      Out << C;
    else
      Out << '\\' << hexdigit(C >> 4) << hexdigit(C & 0x0F);
  }
  return Out;
}

template <bool IgnoreQuotes>
static raw_ostream &PrintCStyleEscapedString(StrRef Name, raw_ostream &Out) {
  for (unsigned char C : Name) {
    if (!IgnoreQuotes && (C == '\\' || C == '\"'))
      Out << '\\' << C;
    else if (isPrint(C))
      Out << C;
    else {
      switch (C) {
      case u8('\a'):
        Out << "\\a";
        break;
      case u8('\b'):
        Out << "\\b";
        break;
      case u8('\f'):
        Out << "\\f";
        break;
      case u8('\n'):
        Out << "\\n";
        break;
      case u8('\r'):
        Out << "\\r";
        break;
      case u8('\t'):
        Out << "\\t";
        break;
      case u8('\v'):
        Out << "\\v";
        break;
      default:
        Out << "\\x" << hexdigit(C >> 4) << hexdigit(C & 0x0F);
      }
    }
  }
  return Out;
}

raw_ostream &exi::printCStyleEscapedString(StrRef Name, raw_ostream &Out,
                                           bool IgnoreQuotes) {
  if (IgnoreQuotes)
    return PrintCStyleEscapedString<true>(Name, Out);
  else
    return PrintCStyleEscapedString<false>(Name, Out);
}

raw_ostream &exi::printHTMLEscaped(StrRef String, raw_ostream &Out) {
  for (char C : String) {
    if (C == '&')
      Out << "&amp;";
    else if (C == '<')
      Out << "&lt;";
    else if (C == '>')
      Out << "&gt;";
    else if (C == '\"')
      Out << "&quot;";
    else if (C == '\'')
      Out << "&apos;";
    else
      Out << C;
  }
  return Out;
}

raw_ostream &exi::printLowerCase(StrRef String, raw_ostream &Out) {
  for (const char C : String)
    Out << toLower(C);
  return Out;
}

String exi::convertToSnakeFromCamelCase(StrRef input) {
  if (input.empty())
    return "";

  String snakeCase;
  snakeCase.reserve(input.size());
  auto check = [&input](size_t j, function_ref<bool(int)> predicate) {
    return j < input.size() && predicate(input[j]);
  };
  for (size_t i = 0; i < input.size(); ++i) {
    snakeCase.push_back(tolower(input[i]));
    // Handles "runs" of capitals, such as in OPName -> op_name.
    if (check(i, isupper) && check(i + 1, isupper) && check(i + 2, islower))
      snakeCase.push_back('_');
    if ((check(i, islower) || check(i, isdigit)) && check(i + 1, isupper))
      snakeCase.push_back('_');
  }
  return snakeCase;
}

String exi::convertToCamelFromSnakeCase(StrRef input, bool capitalizeFirst) {
  if (input.empty())
    return "";

  String output;
  output.reserve(input.size());

  // Push the first character, capatilizing if necessary.
  if (capitalizeFirst && std::islower(input.front()))
    output.push_back(exi::toUpper(input.front()));
  else
    output.push_back(input.front());

  // Walk the input converting any `*_[a-z]` snake case into `*[A-Z]` camelCase.
  for (size_t pos = 1, e = input.size(); pos < e; ++pos) {
    if (input[pos] == '_' && pos != (e - 1) && std::islower(input[pos + 1]))
      output.push_back(exi::toUpper(input[++pos]));
    else
      output.push_back(input[pos]);
  }
  return output;
}
