//===- rapidxml_fwd.hpp ----------------------------------------0----===//
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

#pragma once

#include <Config/XML.inc>
#include <Support/Allocator.hpp>
#include <Support/Alignment.hpp>

// On MSVC, disable "conditional expression is constant" warning (level 4).
// This warning is almost impossible to avoid with certain types of templated
// Code
#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable : 4127) // Conditional expression is constant
#endif

///////////////////////////////////////////////////////////////////////////
// Pool sizes

#ifndef RAPIDXML_STATIC_POOL_SIZE
// Size of static memory block of MemoryPool.
// Define RAPIDXML_STATIC_POOL_SIZE before including rapidxml.hpp if you want to
// override the default value. No dynamic memory allocations are performed by
// MemoryPool until static memory is exhausted.
# define RAPIDXML_STATIC_POOL_SIZE (64 * 1024)
#endif

#ifndef RAPIDXML_DYNAMIC_POOL_SIZE
// Size of dynamic memory block of MemoryPool.
// Define RAPIDXML_DYNAMIC_POOL_SIZE before including rapidxml.hpp if you want
// to override the default value. After the static block is exhausted, dynamic
// blocks with approximately this size are allocated by MemoryPool.
# define RAPIDXML_DYNAMIC_POOL_SIZE (64 * 1024)
#endif

#ifndef RAPIDXML_ALIGNMENT
// Memory allocation alignment.
// Define RAPIDXML_ALIGNMENT before including rapidxml.hpp if you want to
// override the default value, which is the size of pointer. All memory
// allocations for nodes, attributes and strings will be aligned to this value.
// This must be a power of 2 and at least 1, otherwise MemoryPool will not
// work.
# define RAPIDXML_ALIGNMENT sizeof(void*)
#endif

namespace xml {
// Forward declarations
template <class Ch> class XMLBase;
template <class Ch> class XMLNode;
template <class Ch> class XMLAttribute;
template <class Ch> class XMLDocument;

//! Enumeration listing all node types produced by the parser.
//! Use XMLNode::type() function to query node type.
enum NodeKind : int;

inline constexpr usize kPoolSize = RAPIDXML_DYNAMIC_POOL_SIZE;
inline constexpr usize kAlignVal = RAPIDXML_ALIGNMENT;
inline constexpr exi::Align kAlign(kAlignVal);

using XMLBumpAllocator = exi::BumpPtrAllocatorImpl<
  exi::MallocAllocator, kPoolSize, kPoolSize, 32>;

} // namespace xml

// On MSVC, restore warnings state
#ifdef _MSC_VER
# pragma warning(pop)
#endif
