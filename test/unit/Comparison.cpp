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

static int call_system(const std::string& S, bool do_flush = true) {
  fmt::println(stderr, "Exec: '{}'", S);
  if (do_flush)
    std::cout << std::flush;
  return std::system(S.c_str());
}

static int call_exificent(const std::vector<Str>& args, bool do_flush = true) {
  auto cmd = fmt::format("java -jar {} {}",
    exificent,
    fmt::join(args, " ")
  );
  return call_system(cmd, do_flush);
}

static int call_exificent(const Str& arg = "", bool do_flush = true) {
  return call_exificent(std::vector{arg}, do_flush);
}

namespace {

class Conformance : public testing::Test {
protected:
  void SetUp() override {
    static bool dir_exists = fs::exists(exificent_dir);
    if (!dir_exists)
      GTEST_SKIP() << "Exificent directory could not be found. Skipping.";
    
    static bool file_exists = fs::exists(exificent);
    if (!file_exists)
      GTEST_SKIP() << "Exificent could not be found. Skipping.";
    
    static bool java_exists = (call_exificent("--version") == 0);
    if (!java_exists)
      GTEST_SKIP() << "Java could not be found. Skipping.";
  }
};

TEST_F(Conformance, Encoding) {
  
}

} // namespace `anonymous`
