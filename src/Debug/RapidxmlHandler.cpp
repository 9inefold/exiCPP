//===- Debug/RapidxmlHandler.cpp ------------------------------------===//
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

#include <XML.hpp>
#include <Debug/Format.hpp>
#include <cstdio>
#include <stdexcept>
#include <fmt/color.h>

using namespace exi;

namespace rapidxml {

bool use_exceptions_anyway = false;

[[noreturn]] static void printFinal(const std::string& str) {
  if (!use_exceptions_anyway) {
    fmt::println(stderr, "{}", dbg::styled(
      str, fmt::fg(fmt::color::red))
    );
    dbg::fatalError();
  } else {
    throw std::runtime_error(str);
  }
}

void parse_error_handler(const char* what, void* where) {
  if (where) {
    const auto* cwhere = static_cast<Char*>(where);
    auto out = fmt::format("Rapidxml parse error at '{}': {}", *cwhere, what);
    printFinal(out);
  } else {
    auto out = fmt::format("Rapidxml parse error: {}", what);
    printFinal(out);
  }
}

} // namespace rapidxml
