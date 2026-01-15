//===- Common/StrRef-inl.hpp ----------------------------------------===//
//
// Copyright (C) 2026 Ninefold
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
/// This file implements shared functions for StrRef.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/StrRef.hpp>
#include <Common/bitset.hpp>
#include <algorithm>

namespace exi {

ALWAYS_INLINE usize StrRef::find_first_of(const filter_t& F, usize From) const {
  for (size_type i = std::min(From, size()), e = size(); i != e; ++i)
    if (F.test((unsigned char)data()[i]))
      return i;
  return npos;
}

ALWAYS_INLINE usize StrRef::find_first_not_of(const filter_t& F, usize From) const {
  for (size_type i = std::min(From, size()), e = size(); i != e; ++i)
    if (!F.test((unsigned char)data()[i]))
      return i;
  return npos;
}

ALWAYS_INLINE usize StrRef::find_last_of(const filter_t& F, usize From) const {
  for (size_type i = std::min(From, size()) - 1, e = -1; i != e; --i)
    if (F.test((unsigned char)data()[i]))
      return i;
  return npos;
}

ALWAYS_INLINE usize StrRef::find_last_not_of(const filter_t& F, usize From) const {
  for (size_type i = std::min(From, size()) - 1, e = -1; i != e; --i)
    if (!F.test((unsigned char)data()[i]))
      return i;
  return npos;
}

inline StrRef StrRef::ltrim(const filter_t& F) const {
  return drop_front(std::min(size(), find_first_not_of(F)));
}

inline StrRef StrRef::rtrim(const filter_t& F) const {
  return drop_back(size() - std::min(size(), find_last_not_of(F) + 1));
}

inline StrRef StrRef::trim(const filter_t& F) const {
  return ltrim(F).rtrim(F);
}

inline std::pair<usize, usize> StrRef::find_token(const filter_t& F) const {
  // Figure out where the token starts.
  StrRef::size_type Start = find_first_not_of(F);

  // Find the next occurrence of the delimiter.
  StrRef::size_type End = find_first_of(F, Start);

  return std::make_pair(Start, End);
}

} // namespace exi
