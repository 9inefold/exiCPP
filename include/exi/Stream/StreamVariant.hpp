//===- exi/Stream/BitStreamReader.hpp -------------------------------===//
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
/// This file implements the BitStreamReader class.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/PointerUnion.hpp>
#include <exi/Stream/BitStreamReader.hpp>
#include <exi/Stream/BitStreamWriter.hpp>

namespace exi {

/// Declare a fake stream (for now)
/// May also scrap this and manually align streams, but this may be clearer.
class alignas(8) DummyStream { DummyStream() = delete; };

namespace bitstream {

using BitReader  = BitStreamReader;
using BitWriter  = BitStreamWriter;
using ByteReader = DummyStream;
using ByteWriter = DummyStream;

/// A pointer union of the stream types.
using StreamReader = PointerUnion<BitReader*, ByteReader*>;
/// A pointer union of the stream types.
using StreamWriter = PointerUnion<BitWriter*, ByteWriter*>;

} // namespace bitstream

using bitstream::StreamReader;
using bitstream::StreamWriter;

} // namespace exi
