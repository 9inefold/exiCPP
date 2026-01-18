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
#include <exi/Basic/ExiOptions.hpp>
#include <exi/Basic/XMLManager.hpp>

namespace exi {

/// Options used by the XML dumper.
struct XMLDumpOptions {
  static constexpr ExiOptions::PreserveOpts kPreserveAll = {
    .Comments      = true,
    .DTDs          = true,
    .PIs           = true,
    .Prefixes      = true
  };
  
public:
  /// The stream to write to. Defaults to `outs()`.
  Option<raw_ostream&> OS = std::nullopt;
  /// The size of indentation. Defaults to `2`.
  int IndentScale = 2;
  /// The initial indentation level. Defaults to `0` when a stream isn't
  /// provided, and `1` if one is.
  Option<int> InitialIndent = std::nullopt;
  /// Determines the namespace ordering strategy.
  /// If `true`, namespaces will be in document order.
  /// Otherwise, namespaces will be in shortlex order.
  bool Conforming = false;
  /// If comments, DOCTYPES, and Processing Instructions should be kept.
  ExiOptions::PreserveOpts Preserve = kPreserveAll;
  /// If `<?xml ...?>` should be kept. Defaults to Preserve.PIs.
  Option<bool> PreserveDeclaration = std::nullopt;
  /// If CDATA blocks should be kept untouched or escaped.
  bool PreserveCDATA = true;
  /// Prints extra debugging info.
  bool Debug = false;
};

/// Wrapper for the XML dumper.
struct XMLDump {
  /// Dump XML from the file `Filepath`.
  static void full(XMLManager& Mgr,
                   const Twine& Filepath,
                   const XMLDumpOptions& Opts);
  
  /// Dump XML from the file `Filepath`.
  static void full(XMLManager& Mgr,
                   const Twine& Filepath,
                   Option<raw_ostream&> InOS = std::nullopt,
                   bool Conforming = false,
                   ExiOptions::PreserveOpts Preserve
                     = XMLDumpOptions::kPreserveAll) {
    XMLDump::full(Mgr, Filepath, {
      .OS = InOS,
      .Conforming = Conforming,
      .Preserve = Preserve
    });
  }

  /// Dump XML from the given document.
  static void full(XMLDocument& Doc, const XMLDumpOptions& Opts);

  /// Dump XML from the given document.
  static void full(XMLDocument& Doc,
                   Option<raw_ostream&> InOS = std::nullopt,
                   bool Conforming = false,
                   ExiOptions::PreserveOpts Preserve
                     = XMLDumpOptions::kPreserveAll) {
    XMLDump::full(Doc, {
      .OS = InOS,
      .Conforming = Conforming,
      .Preserve = Preserve
    });
  }
};

} // namespace exi
