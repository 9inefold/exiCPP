//===- exi/Basic/XMLManager.hpp -------------------------------------===//
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
/// This file defines a handler for XML files.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/EnumTraits.hpp>
#include <exi/Basic/FileManager.hpp>
#include <rapidxml_fwd.hpp>

namespace exi {

class XMLManager : public RefCountedBase<XMLManager> {
	FileManager& FS;
	xml::XMLBumpAllocator Alloc; // Shared allocator for XML files.

	EXI_PREFER_TYPE(bool)
	unsigned Immutable : 1; // If the source text will be modified.

	EXI_PREFER_TYPE(bool)
	unsigned Strict : 1; // Disables comment, DOCTYPE, and PI parsing.

public:
	XMLManager() = default;
};

} // namespace exi
