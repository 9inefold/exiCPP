//===- Driver.hpp ---------------------------------------------------===//
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

#pragma once

#include <Common/Option.hpp>
#include <Common/Twine.hpp>
#include <Support/raw_ostream.hpp>
#include <exi/Basic/XMLManager.hpp>

namespace exi {

class WithColor {
  raw_ostream& OS;
  raw_ostream::TiedColor TColor;

public:
  using enum raw_ostream::Colors;
  using Colors = raw_ostream::Colors;

  [[nodiscard]] WithColor(raw_ostream& OS,
                          Colors Color = SAVEDCOLOR) :
   OS(OS), TColor(OS.getTiedColor()) {
    OS.changeColor(Color);
  }
  ~WithColor() { OS.changeColor(TColor); }

  template <typename T> WithColor &operator<<(T& O) {
    OS << O;
    return *this;
  }
  template <typename T> WithColor &operator<<(const T& O) {
    OS << O;
    return *this;
  }
};

} // namespace exi

namespace root {

void tests_main(int Argc, char* Argv[]);

void FullXMLDump(exi::XMLManager& Mgr,
                 const exi::Twine& Filepath,
                 exi::Option<exi::raw_ostream&> InOS = std::nullopt,
                 bool DbgPrintTypes = false);

void FullXMLDump(exi::XMLDocument& Doc,
                 exi::Option<exi::raw_ostream&> InOS = std::nullopt,
                 bool DbgPrintTypes = false);

} // namespace root
