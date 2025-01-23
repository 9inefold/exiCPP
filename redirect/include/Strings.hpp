//===- Strings.hpp --------------------------------------------------===//
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
/// This file declares utilities for dealing with strings.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Fundamental.hpp>

namespace re {

//////////////////////////////////////////////////////////////////////////
// Internal - Chars

namespace H {
static inline bool is_upper(char C) noexcept {
  return (C >= 'A' && C <= 'Z');
}

static inline bool is_lower(char C) noexcept {
  return (C >= 'a' && C <= 'z');
}

static inline bool is_digit(char C) noexcept {
  return (C >= '0' && C <= '9');
}

static inline bool is_alpha(char C) noexcept {
  return is_upper(C) || is_lower(C);
}

static inline bool is_alnum(char C) noexcept {
  return is_alpha(C) || is_digit(C);
}

static inline bool is_space(char C) noexcept {
  return (C == ' ' || (C - '\t') < 5);
}
} // namespace H

//////////////////////////////////////////////////////////////////////////
// Untyped

void* VMemcpy(void* Dst, const void* Src, usize Len); // In `VMemcpy.cpp`
void* VMemset(void* Dst, u8 Val, usize Len);          // In `VMemset.cpp`
void* VBzero(void* Dst, usize Len);                   // In `VMemset.cpp`

//////////////////////////////////////////////////////////////////////////
// Typed

template <typename T>
T* Memcpy(T* Dst, const T* Src, usize Len) noexcept {
  (void) VMemcpy(Dst, Src, sizeof(T) * Len);
  return Dst;
}

template <typename T>
T* Memset(T* Dst, u8 Val, usize Len) noexcept {
  (void) VMemset(Dst, Val, sizeof(T) * Len);
  return Dst;
}

template <typename T>
T* Bzero(T* Dst, u8 Val, usize Len) noexcept {
  (void) VBzero(Dst, sizeof(T) * Len);
  return Dst;
}

//////////////////////////////////////////////////////////////////////////
// Strings

usize Strlen(const char* Str);
usize Strnlen(const char* Str, usize Max);

int Strcmp(const char* LHS, const char* RHS);
int Strncmp(const char* LHS, const char* RHS, usize Max);

int StrcmpInsensitive(const char* LHS, const char* RHS);
int StrncmpInsensitive(const char* LHS, const char* RHS, usize Max);

char* Strchr(char* Str, u8 Val);
const char* Strchr(const char* Str, u8 Val);

char* StrchrInsensitive(char* Str, u8 Val);
const char* StrchrInsensitive(const char* Str, u8 Val);

inline bool Strequal(const char* LHS, const char* RHS) {
  return Strcmp(LHS, RHS) == 0;
}
inline bool Strequal(const char* LHS, const char* RHS, usize Max) {
  return Strncmp(LHS, RHS, Max) == 0;
}

inline bool StrequalInsensitive(const char* LHS, const char* RHS) {
  return StrcmpInsensitive(LHS, RHS) == 0;
}
inline bool StrequalInsensitive(const char* LHS, const char* RHS, usize Max) {
  return StrncmpInsensitive(LHS, RHS, Max) == 0;
}

} // namespace re
