//===- exi/Basic/XMLDumper.hpp --------------------------------------===//
//
// Copyright (C) 2024-2026 Ninefold
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

#include <core/Common/Option.hpp>
#include <core/Common/Twine.hpp>
#include <core/Support/WithColor.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/XMLManager.hpp>

namespace exi {

struct XMLDumpOptions {
  Option<raw_ostream&> OS = std::nullopt;
  int IdentLevel = 2; // Currently unused...
  bool Conforming = false;
  bool Debug = false;
};

struct XMLDump {
  static void full(XMLManager& Mgr,
                   const Twine& Filepath,
                   Option<raw_ostream&> InOS = std::nullopt,
                   bool DbgPrintTypes = false,
                   bool Conforming = false);

  static void full(XMLDocument& Doc,
                   Option<raw_ostream&> InOS = std::nullopt,
                   bool DbgPrintTypes = false,
                   bool Conforming = false);

  static void full(XMLManager& Mgr,
                   const Twine& Filepath,
                   const XMLDumpOptions& Opts) {
    XMLDump::full(Mgr, Filepath, Opts.OS, Opts.Debug, Opts.Conforming);
  }

  static void full(XMLDocument& Doc, const XMLDumpOptions& Opts) {
    XMLDump::full(Doc, Opts.OS, Opts.Debug, Opts.Conforming);
  }
};

} // namespace exi
