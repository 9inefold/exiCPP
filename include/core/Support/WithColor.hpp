//===- Support/WithColor.hpp ----------------------------------------===//
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
/// This file implements WithColor, a ScopedSave for raw_ostream color.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Support/raw_ostream.hpp>

// TODO: Implement ColorMode?

namespace exi {

// Symbolic names for various syntax elements.
enum class HighlightColor {
  Address,
  String,
  Tag,
  Attribute,
  Enumerator,
  Macro,
  Error,
  Warning,
  Note,
  Remark
};

/// An RAII object that temporarily switches an output stream to a specific color.
class WithColor {
  raw_ostream& OS;
  raw_ostream::TiedColor TColor;
public:
  using enum raw_ostream::Colors;
  using Colors = raw_ostream::Colors;

  /// To be used like this: `WithColor(OS) << COLOR_CHANGE << "text";`
  /// @param OS The output stream
  [[nodiscard]] WithColor(raw_ostream& OS) :
   OS(OS), TColor(OS.getTiedColor()) {
  }

  /// To be used like this: `WithColor(OS, raw_ostream::BLACK) << "text";`
  /// @param OS The output stream
  /// @param Color ANSI color to use
  [[nodiscard]] WithColor(raw_ostream& OS, Colors Color) : WithColor(OS) {
    OS.changeColor(Color);
  }

  /// To be used like this: `WithColor(OS, HighlightColor::String) << "text";`
  /// @param OS The output stream
  /// @param S Symbolic name for syntax element to color
  [[nodiscard]] WithColor(raw_ostream& OS, HighlightColor S)  : WithColor(OS) {
    this->SetHighlight(OS, S);
  }
  ~WithColor() { OS.changeColor(TColor); }

  raw_ostream& get() { return OS; }
  operator raw_ostream&() { return OS; }
  raw_ostream* operator->() { return &OS; }

  template <typename T> WithColor& operator<<(T& O) {
    OS << O;
    return *this;
  }
  template <typename T> WithColor& operator<<(const T& O) {
    OS << O;
    return *this;
  }

  /// Sets the highlight color for `OS`.
  static raw_ostream& SetHighlight(raw_ostream& OS, HighlightColor S);
};

EXI_INLINE raw_ostream& operator<<(raw_ostream& OS, HighlightColor S) {
  return WithColor::SetHighlight(OS, S);
}

} // namespace exi
