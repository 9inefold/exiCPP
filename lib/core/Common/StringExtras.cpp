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


#include <Common/StringExtras.hpp>
#include <Common/SmallVec.hpp>
#include <Support/raw_ostream.hpp>
#include <cctype>

using namespace exi;

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
/// token at any of the characters that appear in the Delimiters string.  If
/// there are no tokens in the source string, an empty string is returned.
/// The function returns a pair containing the extracted token and the
/// remaining tail string.
std::pair<StrRef, StrRef> exi::getToken(StrRef Source,
                                               StrRef Delimiters) {
  // Figure out where the token starts.
  StrRef::size_type Start = Source.find_first_not_of(Delimiters);

  // Find the next occurrence of the delimiter.
  StrRef::size_type End = Source.find_first_of(Delimiters, Start);

  return std::make_pair(Source.slice(Start, End), Source.substr(End));
}

/// SplitString - Split up the specified string according to the specified
/// delimiters, appending the result fragments to the output list.
void exi::SplitString(StrRef Source,
                      SmallVecImpl<StrRef> &OutFragments,
                      StrRef Delimiters) {
  std::pair<StrRef, StrRef> S = getToken(Source, Delimiters);
  while (!S.first.empty()) {
    OutFragments.push_back(S.first);
    S = getToken(S.second, Delimiters);
  }
}

void exi::printEscapedString(StrRef Name, raw_ostream &Out) {
  for (unsigned char C : Name) {
    if (C == '\\')
      Out << '\\' << C;
    else if (isPrint(C) && C != '"')
      Out << C;
    else
      Out << '\\' << hexdigit(C >> 4) << hexdigit(C & 0x0F);
  }
}

void exi::printHTMLEscaped(StrRef String, raw_ostream &Out) {
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
}

void exi::printLowerCase(StrRef String, raw_ostream &Out) {
  for (const char C : String)
    Out << toLower(C);
}

Str exi::convertToSnakeFromCamelCase(StrRef input) {
  if (input.empty())
    return "";

  Str snakeCase;
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

Str exi::convertToCamelFromSnakeCase(StrRef input, bool capitalizeFirst) {
  if (input.empty())
    return "";

  Str output;
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
