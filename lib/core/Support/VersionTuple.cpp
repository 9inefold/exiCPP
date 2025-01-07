//===- Support/VersionTuple.cpp -------------------------------------===//
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
//
// This file implements the VersionTuple class, which represents a version in
// the form major[.minor[.subminor]].
//
//===----------------------------------------------------------------===//

#include <Support/VersionTuple.hpp>
#include <Common/StrRef.hpp>
#include <Support/raw_ostream.hpp>
#include <cassert>

using namespace exi;

String VersionTuple::getAsString() const {
  String Result;
  {
    exi::raw_string_ostream Out(Result);
    Out << *this;
  }
  return Result;
}

raw_ostream &exi::operator<<(raw_ostream &Out, const VersionTuple &V) {
  Out << V.getMajor();
  if (Option<unsigned> Minor = V.getMinor())
    Out << '.' << *Minor;
  if (Option<unsigned> Subminor = V.getSubminor())
    Out << '.' << *Subminor;
  if (Option<unsigned> Build = V.getBuild())
    Out << '.' << *Build;
  return Out;
}

static bool parseInt(StrRef &input, unsigned &value) {
  assert(value == 0);
  if (input.empty())
    return true;

  char next = input[0];
  input = input.substr(1);
  if (next < '0' || next > '9')
    return true;
  value = (unsigned)(next - '0');

  while (!input.empty()) {
    next = input[0];
    if (next < '0' || next > '9')
      return false;
    input = input.substr(1);
    value = value * 10 + (unsigned)(next - '0');
  }

  return false;
}

bool VersionTuple::tryParse(StrRef input) {
  unsigned major = 0, minor = 0, micro = 0, build = 0;

  // Parse the major version, [0-9]+
  if (parseInt(input, major))
    return true;

  if (input.empty()) {
    *this = VersionTuple(major);
    return false;
  }

  // If we're not done, parse the minor version, \.[0-9]+
  if (input[0] != '.')
    return true;
  input = input.substr(1);
  if (parseInt(input, minor))
    return true;

  if (input.empty()) {
    *this = VersionTuple(major, minor);
    return false;
  }

  // If we're not done, parse the micro version, \.[0-9]+
  if (!input.consume_front("."))
    return true;
  if (parseInt(input, micro))
    return true;

  if (input.empty()) {
    *this = VersionTuple(major, minor, micro);
    return false;
  }

  // If we're not done, parse the micro version, \.[0-9]+
  if (!input.consume_front("."))
    return true;
  if (parseInt(input, build))
    return true;

  // If we have characters left over, it's an error.
  if (!input.empty())
    return true;

  *this = VersionTuple(major, minor, micro, build);
  return false;
}
