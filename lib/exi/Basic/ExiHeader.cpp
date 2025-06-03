//===- exi/Basic/ExiHeader.cpp --------------------------------------===//
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

#include <exi/Basic/ExiHeader.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/ErrorCodes.hpp>

#define DEBUG_TYPE "ExiHeader"

using namespace exi;

ExiError exi::ValidateHeaderOnly(const ExiHeader& Header) {
  // Ensure options are provided.
  // TODO: If permissive mode is added, allow deferred validation.
  if (!Header.Opts) {
    LOG_ERROR("options must be provided!");
    // FIXME: Incorrect error message for encoding
    return ExiError::Header();
  }

  // Validate version.
  if (Header.IsPreviewVersion)
    return ExiError::HeaderVer();
  else if (Header.ExiVersion > kCurrentExiVersion)
    return ExiError::HeaderVer(Header.ExiVersion);
  else if (Header.ExiVersion == 0)
    return ExiError::HeaderVer(0);

  return ExiError::OK;
}

ExiError exi::ValidateHeader(const ExiHeader& Header) {
  exi_try(exi::ValidateHeaderOnly(Header));
  return exi::ValidateOptions(*Header.Opts);
}

ExiError exi::FixupAndValidateHeader(const ExiHeader& Header) {
  // TODO: If permissive, set to closest version?
  exi_try(exi::ValidateHeaderOnly(Header));
  return exi::FixupAndValidateOptions(*Header.Opts);
}
