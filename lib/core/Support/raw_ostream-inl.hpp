//===- Support/raw-ostream-inl.hpp ----------------------------------===//
//
// Copyright (C) 2024-2025 Ninefold
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

#include <Support/raw_ostream.hpp>
#include <Support/Process.hpp>

namespace exi {

template <char C>
inline raw_ostream &raw_ostream::write_padding(unsigned NumChars) {
  static const char Chars[] = {C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,
                               C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,
                               C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,
                               C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,
                               C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C};

  // Usually the indentation is small, handle it with a fastpath.
  if (NumChars < std::size(Chars))
    return this->write(Chars, NumChars);

  while (NumChars) {
    unsigned NumToWrite = std::min(NumChars, (unsigned)std::size(Chars) - 1);
    this->write(Chars, NumToWrite);
    NumChars -= NumToWrite;
  }
  return *this;
}

inline void raw_ostream::write_colorcode([[maybe_unused]] enum Colors Color) {
#if !EXI_DEBUG_OSCOLOR
  exi_unreachable("do not call this without EXI_DEBUG_OSCOLOR=1");
#else
  if (Color == SAVEDCOLOR)
    Color = RESET;
  const char *colorcode =
    sys::Process::OutputColor(
      static_cast<char>(Color), false, false);
  if (colorcode)
    this->write(colorcode, std::strlen(colorcode));
#endif // EXI_DEBUG_OSCOLOR
}

ALWAYS_INLINE void raw_ostream::send_colorcode(const char *colorcode) {
#if !EXI_DEBUG_OSCOLOR
  this->write(colorcode, std::strlen(colorcode));
#else
  this->write_colorcode(BRIGHT_WHITE);
  this->write_escaped(StrRef(colorcode, std::strlen(colorcode)));
  this->write_colorcode(SAVEDCOLOR);
#endif // EXI_DEBUG_OSCOLOR
}

inline raw_ostream &raw_ostream::write_color(
 enum Colors Color, bool Bold, bool BG) {
#if EXI_HAS_SYS_IMPL
  const char *colorcode =
      (Color == SAVEDCOLOR)
          ? sys::Process::OutputBold(BG)
          : sys::Process::OutputColor(static_cast<char>(Color), Bold, BG);
  if (colorcode) {
    // TODO: Handle RESET better
#if 0
    if (*colorcode == '\033') {
      const auto Len = std::strlen(colorcode);
      this->write(colorcode, Len);
      this->write(colorcode + 1, Len - 1);
      this->write("\033[0m", sizeof("\033[0m") - 1);
      return *this;
    }
#endif
    this->send_colorcode(colorcode);
  }
#endif // EXI_HAS_SYS_IMPL
  return *this;
}

ALWAYS_INLINE raw_ostream &raw_ostream::reset_color() {
#if EXI_HAS_SYS_IMPL
  if (const char *colorcode = sys::Process::ResetColor())
    this->send_colorcode(colorcode);
#endif // EXI_HAS_SYS_IMPL
  return *this;
}

} // namespace exi
