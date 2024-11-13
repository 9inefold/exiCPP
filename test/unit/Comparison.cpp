//===- unit/Comparison.cpp ------------------------------------------===//
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



#include "Testing.hpp"

#include <exicpp/Filesystem.hpp>
#include <exicpp/Options.hpp>
#include <exicpp/XML.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <cstdlib>
#include <STL.hpp>

using namespace exi;

namespace {

#ifdef WIN32
static constexpr StrRef shell_eat = "nul";
#else
static constexpr StrRef shell_eat = "/dev/null";
#endif

struct System {
  Str formatCall(const Str& S) const;
  int call(const Str& S, bool do_flush = true) const;
  int callExificent(const Vec<Str>& args, bool do_flush = true) const;
  int callExificent(const Str& arg = "", bool do_flush = true) const;

  int operator()(const Str& S, bool do_flush = true) const {
    return this->call(S, do_flush);
  }

public:
  StrRef out = "";
  StrRef err = "";
};

Str System::formatCall(const Str& S) const {
  if (out.empty()) {
    if (err.empty())
      return S;
    else
      return fmt::format("{} 2>{}", S, err);
  } else {
    if (err.empty())
      return fmt::format("{} >{}", S, out);
    else
      return fmt::format("{} >{} 2>{}", S, out, err);
  }
}

int System::call(const Str& S, bool do_flush) const {
  if (do_flush)
    std::flush(std::cout);
  Str cmd = this->formatCall(S);
  // fmt::println(stderr, "Exec: '{}'", cmd);
  return std::system(cmd.c_str());
}

int System::callExificent(const Vec<Str>& args, bool do_flush) const {
  auto cmd = fmt::format("java -jar {} {}",
    exificent,
    fmt::join(args, " ")
  );
  return this->call(cmd, do_flush);
}

int System::callExificent(const Str& arg, bool do_flush) const {
  return this->callExificent(Vec{arg}, do_flush);
}

//////////////////////////////////////////////////////////////////////////
// Predefined

namespace shell {
  /// Eat both `stdout` and `stderr`.
  static constexpr System None {shell_eat, shell_eat};
  /// Eat only `stdout`, let `stderr` print.
  static constexpr System Err  {shell_eat, ""};
  /// Allow both `stdout` and `stderr` to print.
  static constexpr System All  {"", ""};
} // namespace shell

//======================================================================//
// Options
//======================================================================//

struct Opts {
  exi::Options forExip() const;
  Vec<Str> forExificent() const;
public:
  unsigned Compression:           1; 
  unsigned Strict:                1; 
  unsigned Fragment:              1; 
  unsigned SelfContained:         1;

  unsigned BitPacked:             1;
  unsigned ByteAligned:           1;
  unsigned PreCompression:        1;

  unsigned PreserveComments:      1;     
  unsigned PreservePIs:           1;          
  unsigned PreserveDTD:           1;          
  unsigned PreservePrefixes:      1;     
  unsigned PreserveLexicalValues: 1;
};

#define SET_OPT(flag, val) do { \
  if (!!this->flag) O.set(val); \
} while(0)

exi::Options Opts::forExip() const {
  exi::Options O {};

  SET_OPT(Compression,            EnumOpt::Compression);
  SET_OPT(Strict,                 EnumOpt::Strict);
  SET_OPT(Fragment,               EnumOpt::Fragment);
  SET_OPT(SelfContained,          EnumOpt::SelfContained);

  SET_OPT(ByteAligned,            Align::ByteAlignment);
  SET_OPT(PreCompression,         Align::PreCompression);
  SET_OPT(BitPacked,              Align::BitPacked);

  SET_OPT(PreserveComments,       Preserve::Comments);
  SET_OPT(PreservePIs,            Preserve::PIs);
  SET_OPT(PreserveDTD,            Preserve::DTD);
  SET_OPT(PreservePrefixes,       Preserve::Prefixes);
  SET_OPT(PreserveLexicalValues,  Preserve::LexicalValues);

#undef SET_OPT
  return O;
}

#define SET_OPT(flag, val) do { \
  if (!!this->flag) O.emplace_back(val); \
} while(0)

Vec<Str> Opts::forExificent() const {
  Vec<Str> O;

  SET_OPT(Compression,            "compression");
  SET_OPT(Strict,                 "strict");
  SET_OPT(Fragment,               "fragment");
  // SET_OPT(SelfContained,          "selfContained");

  if (!this->BitPacked)
    SET_OPT(ByteAligned,          "bytePacked");
  SET_OPT(PreCompression,         "preCompression");

  SET_OPT(PreserveComments,       "preserveComments");
  SET_OPT(PreservePIs,            "preservePIs");
  SET_OPT(PreserveDTD,            "preserveDTDs");
  SET_OPT(PreservePrefixes,       "preservePrefixes");
  SET_OPT(PreserveLexicalValues,  "preserveLexicalValues");

#undef SET_OPT
  return O;
}

//======================================================================//
// Suite
//======================================================================//

class ConformanceBase : public testing::Test {
protected:
  void SetUp() override {
    static bool dir_exists = fs::exists(exificent_dir);
    if (!dir_exists)
      GTEST_SKIP() << "Exificent directory could not be found. Skipping.";
    
    static bool file_exists = fs::exists(exificent);
    if (!file_exists)
      GTEST_SKIP() << "Exificent could not be found. Skipping.";
    
    static bool java_exists = CheckJavaInstall();
    if (!java_exists) {
      fmt::println(stderr, "Ensure Java has been installed!");
      GTEST_SKIP() << "Java could not be found. Skipping.";
    }
  }

  static bool CheckJavaInstall();
};

bool ConformanceBase::CheckJavaInstall() {
  // These are on both UNIX and modern Windows.
  int ret = shell::Err.call("which java", false);
#ifdef _WIN32
  if (ret != 0) {
    // If invalid for any reason, check where.
    ret = shell::Err.call("where /q java", false);
  }
#endif
  return (ret == 0);
}

class Conformance : public ConformanceBase {};

//======================================================================//
// Testing
//======================================================================//

TEST_F(Conformance, Encoding) {
  ASSERT_EQ(1,1);
}

} // namespace `anonymous`
