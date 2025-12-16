//===- Support/GetFileRelStart.hpp ----------------------------------===//
//
// Copyright (C) 2024 Ninefold
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

#include <Common/Features.hpp>
#include <Common/StrRef.hpp>

namespace exi {

/// This has to be in its own file because clang uses different path separators
/// in headers.
inline consteval StrRef GetFileRelStartHdr() {
#if EXI_HAS_BUILTIN(__builtin_FILE)
  std::string_view File = __builtin_FILE();
#else
  std::string_view File = __FILE__;
#endif
  const usize Pos = File.find("exiCPP");
  if (Pos == File.npos)
    return ""_str;
  return StrRef(File.data(), Pos);
}

inline constexpr StrRef kHdrFilePfx = GetFileRelStartHdr();

} // namespace exi
