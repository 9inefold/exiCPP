//===- exi/Encode/HeaderEncoder.hpp ----------------------------------===//
//
// Copyright (C) 2025 Eightfold
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
/// This file implements encoding of the EXI Header to a stream.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Poly.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Stream/OrderedWriter.hpp>

namespace exi {

struct ExiHeader;

/// Decodes an EXI header given an arbitrary stream.
ExiError encodeHeader(const ExiHeader& Header, OrdWriter& Strm);

// TODO: Add encode that pulls options from XML.

} // namespace exi
