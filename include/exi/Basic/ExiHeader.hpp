//===- exi/Basic/ExiHeader.hpp --------------------------------------===//
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
///
/// \file
/// This file defines the EXI header.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/MaybeBox.hpp>
#include <core/Common/D/Str.hpp>
#include <exi/Basic/ExiOptions.hpp>

namespace exi {

inline constexpr u32 kCurrentExiVersion = 1;

/// Exi header without the options.
struct ExiHeaderOnly {
  /// If the file begins with "$EXI".
  bool HasCookie : 1 = false;

  /// If the header has options.
  bool HasOptions : 1 = true;

  /// If the version is a preview.
  bool IsPreviewVersion : 1 = false;

  /// If the version is a preview
  u32 ExiVersion = kCurrentExiVersion;
};

/// Exi header, including the options.
struct ExiHeader {
  EXI_PREFER_TYPE(bool)
  /// If the file begins with "$EXI".
  u32 HasCookie : 1 = false;

  EXI_PREFER_TYPE(bool)
  /// If the header has options.
  u32 HasOptions : 1 = true;

  EXI_PREFER_TYPE(bool)
  /// If the version is a preview.
  u32 IsPreviewVersion : 1 = false;

  /// If the version is a preview
  u32 ExiVersion : 30 = kCurrentExiVersion;

  /// Options used by the EXI processor.
  MaybeBox<ExiOptions> Opts;
};

/// Will verify header validity without checking options.
ExiError ValidateHeaderOnly(ExiHeaderOnly Header);

/// Will verify header validity without checking options.
ExiError ValidateHeaderOnly(const ExiHeader& Header);

/// Will verify header validity without modification.
ExiError ValidateHeader(const ExiHeader& Header);

/// Will verify header validity, passing `Opts` to be fixed up.
ExiError FixupAndValidateHeader(const ExiHeader& Header);

/// Extracts `ExiHeader` data to an `ExiHeaderOnly`.
inline ExiHeaderOnly GetHeaderOnlyData(const ExiHeader& Data) {
  ExiHeaderOnly Out {};
  Out.HasCookie         = Data.HasCookie;
  Out.HasOptions        = Data.HasOptions;
  Out.IsPreviewVersion  = Data.IsPreviewVersion;
  Out.ExiVersion        = Data.ExiVersion;
  return Out;
}

/// Sets `ExiHeader` data with an `ExiHeaderOnly`.
inline void SetHeaderOnlyData(ExiHeader& Out, ExiHeaderOnly Data) {
  Out.HasCookie         = Data.HasCookie;
  Out.HasOptions        = Data.HasOptions;
  Out.IsPreviewVersion  = Data.IsPreviewVersion;
  Out.ExiVersion        = Data.ExiVersion;
}

//////////////////////////////////////////////////////////////////////////
// Mangling

/// Produces a unique string from the header values.
String exi_mangle_header(const ExiHeader& Header);

/// Produces a unique string from the header values.
/// @returns `true` on success.
bool exi_demangle_header(ExiHeader& Header, StrRef Sym);

} // namespace exi
