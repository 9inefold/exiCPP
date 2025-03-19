//===- exi/Basic/BodyDecoder.cpp ------------------------------------===//
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
/// This file implements decoding of the EXI body from a stream.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/DenseMap.hpp>
#include <core/Common/StringMap.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Decode/HeaderDecoder.hpp>
#include <exi/Stream/StreamVariant.hpp>
#include <variant>

namespace exi {

class ExiDecoder {
public:
  using Reader = std::variant<std::monostate, 
    bitstream::BitReader, bitstream::ByteReader>;

private:
  StreamReader Reader;
  ReaderVariant InlineReader;

public:
  ExiDecoder() = default;
};

} // namespace exi
