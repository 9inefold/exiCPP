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

#include <STL.hpp>

#include <cstdlib>
#include <utility>

using namespace exi;

using String = std::string;

namespace {

#ifdef WIN32
static constexpr StrRef shell_eat = "nul";
#else
static constexpr StrRef shell_eat = "/dev/null";
#endif

struct System {
  String formatCall(const String& S) const;
  int call(const String& S, bool do_flush = true) const;
  int callExificent(const Vec<String>& args, bool do_flush = true) const;
  int callExificent(const String& arg = "", bool do_flush = true) const;

  int operator()(const String& S, bool do_flush = true) const {
    return this->call(S, do_flush);
  }

public:
  StrRef out = "";
  StrRef err = "";
};

String System::formatCall(const String& S) const {
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

int System::call(const String& S, bool do_flush) const {
  if (do_flush)
    std::flush(std::cout);
  String cmd = this->formatCall(S);
  // fmt::println(stderr, "Exec: '{}'", cmd);
  return std::system(cmd.c_str());
}

int System::callExificent(const Vec<String>& args, bool do_flush) const {
  auto cmd = fmt::format("java -jar {} {}",
    exificent,
    fmt::join(args, " ")
  );
  return this->call(cmd, do_flush);
}

int System::callExificent(const String& arg, bool do_flush) const {
  // Explicitly passing the allocator to avoid an ICE pre gcc 12.0
  using A = typename Vec<String>::allocator_type;
  return this->callExificent(Vec({arg}, A{}), do_flush);
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
  String getName() const;
  exi::Options forExip() const;
  Vec<String> forExificent() const;
public:
  unsigned Compression:           1 = false;
  unsigned Strict:                1 = false;
  unsigned Fragment:              1 = false;
  unsigned SelfContained:         1 = false;

  unsigned BitPacked:             1 = true;
  unsigned ByteAligned:           1 = false;
  unsigned PreCompression:        1 = false;

  unsigned PreserveComments:      1 = false;
  unsigned PreservePIs:           1 = false;
  unsigned PreserveDTD:           1 = false;
  unsigned PreservePrefixes:      1 = false;
  unsigned PreserveLexicalValues: 1 = false;
};

#define SET_OPT(flag, bl, val) do { \
  if (!!this->flag) { \
    O.append(val); \
    bl = true; \
  } \
} while(0)

String Opts::getName() const {
  String O;
  
  bool opt = false;
  O.append("Opt");
  SET_OPT(Compression,            opt, "C");
  SET_OPT(Strict,                 opt, "S");
  SET_OPT(Fragment,               opt, "F");
  SET_OPT(SelfContained,          opt, "I");
  if (!opt) O.append("None");
  O.push_back('_');

  bool align = false;
  O.append("Al");
  if (!this->BitPacked && this->ByteAligned) {
    SET_OPT(ByteAligned,          align, "Byte");
  } else {        
    SET_OPT(BitPacked,            align, "Bit");
  }       
  SET_OPT(PreCompression,         align, "Pre");
  O.push_back('_');

  bool preserve = false;
  O.append("Ps");
  SET_OPT(PreserveComments,       preserve, "C");
  SET_OPT(PreservePIs,            preserve, "I");
  SET_OPT(PreserveDTD,            preserve, "D");
  SET_OPT(PreservePrefixes,       preserve, "P");
  SET_OPT(PreserveLexicalValues,  preserve, "L");
  if (!opt) O.append("None");

#undef SET_OPT
  return O;
}

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

Vec<String> Opts::forExificent() const {
  Vec<String> O;

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

class Suite : public testing::Test {
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

bool Suite::CheckJavaInstall() {
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

//======================================================================//
// Testing
//======================================================================//

class Encoding : public Suite {};

std::pair<fs::path, fs::path> getValidXMLPaths() {
  fs::path curr = fs::current_path();
  assert(curr.stem() == "test");
  fs::path dir = curr / "xmltest/valid/sa";
  assert(fs::exists(dir));
  return {dir, dir / "out"};
}

Option<fs::path> checkXMLTest(const fs::directory_entry& entry) {
  if (!entry.is_regular_file())
    return std::nullopt;
  fs::path file = entry.path();
  if (file.extension() != ".xml");
    return std::nullopt;
  return {std::move(file)};
}

TEST_F(Encoding, PsNone_BytePk) {
  auto [in, out] = getValidXMLPaths();
  for (const auto& entry : fs::directory_iterator{in}) {
    auto file = checkXMLTest(entry);
    if (!file)
      continue;

  }
}

} // namespace `anonymous`
