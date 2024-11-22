//===- Filesystem.cpp -----------------------------------------------===//
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

#include <Filesystem.hpp>
#include <Debug/Format.hpp>
#include <cstdlib>

using namespace exi;

static Str simple_mbconvert(const std::wstring& str, Char c = '?') {
  Str outstr;
  outstr.reserve(str.size());
  for (wchar_t wc : str) {
    if (wc < 128)
      outstr.push_back(Char(wc));
    else
      outstr.push_back(c);
  }
  return outstr;
}

namespace exi {

Str to_multibyte(const std::wstring& str) {
  Str outstr;
  const std::size_t wsize = str.size() + 1;
  outstr.resize(wsize * 2);
  const auto len = std::wcstombs(outstr.data(), str.data(), outstr.size());
  if EXICPP_UNLIKELY(len == static_cast<std::size_t>(-1)) {
    const Str errstr = simple_mbconvert(str);
    LOG_ERROR("Unable to correctly format '{}'.", errstr);
    return errstr;
  }
  outstr.resize(len);
  return outstr;
}

} // namespace exi
