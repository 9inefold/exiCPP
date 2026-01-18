//===- Support/WithColor.cpp ----------------------------------------===//
//
// Copyright (C) 2025 Ninefold
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
/// This file implements WithColor, a ScopedSave for raw_ostream color.
///
//===----------------------------------------------------------------===//

#include <Support/WithColor.hpp>
#include <Support/raw_ostream-inl.hpp>

using namespace exi;

raw_ostream& WithColor::SetHighlight(raw_ostream& OS, HighlightColor S) {
  if (!OS.has_colors())
    return OS;
  switch (S) {
  case HighlightColor::Address:
    return OS.changeColor(Colors::YELLOW);
  case HighlightColor::String:
    return OS.changeColor(Colors::GREEN);
  case HighlightColor::Tag:
    return OS.changeColor(Colors::BLUE);
  case HighlightColor::Attribute:
    return OS.changeColor(Colors::CYAN);
  case HighlightColor::Enumerator:
    return OS.changeColor(Colors::MAGENTA);
  case HighlightColor::Macro:
    return OS.changeColor(Colors::RED);
  case HighlightColor::Error:
    return OS.changeColor(Colors::RED, true);
  case HighlightColor::Warning:
    return OS.changeColor(Colors::MAGENTA, true);
  case HighlightColor::Note:
    return OS.changeColor(Colors::BLACK, true);
  case HighlightColor::Remark:
    return OS.changeColor(Colors::BLUE, true);
  }

  exi_unreachable("Invalid HighlightColor?");
}

#if EXI_DEBUG_OSCOLOR
static constinit int withcolorDepth = 0;

ALWAYS_INLINE void WithColor::printDepth(char C) {
  if (!OS.prepare_colors())
    return;
  OS.write_colorcode(BRIGHT_CYAN);
  OS << '[' << C << withcolorDepth << ']';
  if (OS.prepare_colors())
    OS.write_colorcode(RESET);
}

void WithColor::incDepth() {
  ++withcolorDepth;
  this->printDepth('+');
}

void WithColor::decDepth() {
  this->printDepth('-');
  --withcolorDepth;
  exi_assert(withcolorDepth >= 0);
}
#endif
