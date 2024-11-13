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
#include <exicpp/XML.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <cstdlib>
#include <vector>

using namespace exi;

namespace {

#ifdef WIN32
static constexpr std::string_view shell_eat = "nul";
#else
static constexpr std::string_view shell_eat = "/dev/null";
#endif

struct System {
  std::string formatCall(const std::string& S) const;
  int call(const std::string& S, bool do_flush = true) const;
  int callExificent(const std::vector<Str>& args, bool do_flush = true) const;
  int callExificent(const Str& arg = "", bool do_flush = true) const;

  int operator()(const std::string& S, bool do_flush = true) const {
    return this->call(S, do_flush);
  }

public:
  std::string_view out = "";
  std::string_view err = "";
};

std::string System::formatCall(const std::string& S) const {
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

int System::call(const std::string& S, bool do_flush) const {
  if (do_flush)
    std::flush(std::cout);
  std::string cmd = this->formatCall(S);
  // fmt::println(stderr, "Exec: '{}'", cmd);
  return std::system(cmd.c_str());
}

int System::callExificent(const std::vector<Str>& args, bool do_flush) const {
  auto cmd = fmt::format("java -jar {} {}",
    exificent,
    fmt::join(args, " ")
  );
  return this->call(cmd, do_flush);
}

int System::callExificent(const Str& arg, bool do_flush) const {
  return this->callExificent(std::vector{arg}, do_flush);
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


};

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
  
}

} // namespace `anonymous`
