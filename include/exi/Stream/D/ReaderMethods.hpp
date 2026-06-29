//===- exi/Stream/D/ReaderMethods.hpp -------------------------------===//
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

#pragma once

// HACK: Remove ReaderMethods if this is bumped to C++23

/*
def split_macro_text(s: str) -> tuple[list[str], str]:
  splits = [l for l in s.splitlines() if not l.lstrip().startswith('//')]
  return splits[:-1], splits[-1]

def gen_macro(s: str, /, find_largest=False):
  MIN_LINE_LEN = 80
  LINE_LEN = MIN_LINE_LEN - 2
  splits, last_line = split_macro_text(s)
  out: list[str] = []
  # Find the length we want
  if find_largest:
    largest_line = max([len(l) for l in splits])
    if largest_line > LINE_LEN:
      LINE_LEN = largest_line
  for line in splits:
    if len(line) > (LINE_LEN - 1):
      out.append(line + ' \\')
      continue
    # Add the line extras
    padding = LINE_LEN - len(line)
    out.append(line + (' ' * padding) + '\\')
  # Always add the last line
  out.append(last_line)
  return '\n'.join(out)
*/

/// Generates the methods for all readers.
#define EXI_GENERATE_READER_METHODS(CLASSNAME)                                \
private:                                                                      \
  template <typename T>                                                       \
  ALWAYS_INLINE static ExiError SetData(T& Out, const ExiResult<T>& R) {      \
    if EXI_LIKELY(R.is_ok()) {                                                \
      Out = *R;                                                               \
      return ExiError::OK;                                                    \
    } else {                                                                  \
      Out = 0;                                                                \
      return R.error();                                                       \
    }                                                                         \
  }                                                                           \
                                                                              \
public:                                                                       \
  ExiError readBit(bool& Out) {                                               \
    const auto R = this->readBit();                                           \
    return CLASSNAME::SetData(Out, R);                                        \
  }                                                                           \
                                                                              \
  ExiError readByte(u8& Out) {                                                \
    const auto R = this->readByte();                                          \
    return CLASSNAME::SetData(Out, R);                                        \
  }                                                                           \
                                                                              \
  ExiError readBits64(u64& Out, size_type Bits) {                             \
    const auto R = this->readBits64(Bits);                                    \
    return CLASSNAME::SetData(Out, R);                                        \
  }                                                                           \
                                                                              \
  ExiError readUInt(u64& Out) {                                               \
    const auto R = this->readUInt();                                          \
    return CLASSNAME::SetData(Out, R);                                        \
  }                                                                           \
                                                                              \
  template <unsigned Bits>                                                    \
  ExiError readBits(ubit<Bits>& Out) {                                        \
    const auto R = this->readBits64(Bits);                                    \
    Out = ubit<Bits>::FromBits(R.value_or(0));                                \
    return R.error_or(ExiError::OK);                                          \
  }                                                                           \
                                                                              \
  template <unsigned Bits>                                                    \
  ExiResult<ubit<Bits>> readBits() {                                          \
    auto Data = this->readBits64(Bits);                                       \
    if EXI_UNLIKELY(Data.is_err())                                            \
      return Err(Data.error());                                               \
    return ubit<Bits>::FromBits(*Data);                                       \
  }

/// Handles all the methods for `OrderedReader`s.
#define EXI_GENERATE_ORDREADER_METHODS(CLASSNAME)                             \
  using ReaderBase::readBit;                                                  \
  using ReaderBase::readByte;                                                 \
  using ReaderBase::readBits64;                                               \
  using ReaderBase::readUInt;                                                 \
  EXI_GENERATE_READER_METHODS(CLASSNAME)
