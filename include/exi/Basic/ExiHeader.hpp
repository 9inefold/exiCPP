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
#include <exi/Basic/ExiOptions.hpp>

namespace exi {

inline constexpr u32 kCurrentExiVersion = 1;

struct ExiHeader {
  EXI_PREFER_TYPE(bool)
  /// If the file begins with "$EXI".
  u32 HasCookie : 1 = true;

  EXI_PREFER_TYPE(bool)
  /// If the header has options.
  u32 HasOptions : 1 = true;

  EXI_PREFER_TYPE(bool)
  /// If the version is a preview.
  u32 IsPreviewVersion : 1 = false;

  EXI_PREFER_TYPE(bool)
  /// If the version is a preview
  u32 ExiVersion : 30 = kCurrentExiVersion;

  /// Options used by the EXI processor.
  MaybeBox<ExiOptions> Opts;
};

/// Will verify header validity without checking options.
ExiError ValidateHeaderOnly(const ExiHeader& Header);

/// Will verify header validity without modification.
ExiError ValidateHeader(const ExiHeader& Header);
/// Will verify header validity, passing `Opts` to be fixed up.
ExiError FixupAndValidateHeader(const ExiHeader& Header);

} // namespace exi
