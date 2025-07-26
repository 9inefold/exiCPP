//===- exi/Encode/StringTableHandles.hpp -----------------------------===//
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
/// This file implements the opaque handles for the encoder's StringTable.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/OpaqueHandle.hpp>

namespace exi::encode {

class StringTable;

// TODO: Enable macro expansion for EXI_OPAQUE_HANDLE
// See https://stackoverflow.com/questions/42300539/documenting-macros-using-doxygen

/// @typedef STPrefixEntry
/// Typed handle for `StringTable::PrefixEntry`.
EXI_OPAQUE_HANDLE(STPrefixEntry, StringTable);
/// @typedef STURIEntry
/// Typed handle for `StringTable::URIEntry`.
EXI_OPAQUE_HANDLE(STURIEntry, StringTable);
//struct STURIEntry;
/// @typedef STValueEntry
/// Typed handle for `StringTable::ValueEntry`.
EXI_OPAQUE_HANDLE(STValueEntry, StringTable);
/// @typedef QualName
/// Handle for an `InlineString` representing a QName's data as `"URI$ln"`.
EXI_OPAQUE_HANDLE(QualName, StringTable);

} // namespace exi::encode
