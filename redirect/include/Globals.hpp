//===- Globals.hpp --------------------------------------------------===//
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
/// This file declares globals used by exi::redirect.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Fundamental.hpp>

namespace re {

inline bool MIMALLOC_PATCH_IMPORTS = true;
inline bool MIMALLOC_VERBOSE = true;
inline bool NO_PATCH_ERRORS = false;

using _mi_redirect_entry = void(u32 reason);
inline _mi_redirect_entry* mi_redirect_entry = nullptr;

} // namespace re
