//===- exi/Encode/BodyEncoder.hpp ------------------------------------===//
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
/// This file implements encoding of the EXI body to a stream.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/ArrayRef.hpp>
#include <core/Common/DenseMap.hpp>
#include <core/Common/Option.hpp>
#include <core/Common/StringMap.hpp>
#include <core/Common/Vec.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/ExiHeader.hpp>
#include <exi/Basic/StringTables.hpp>
#include <exi/Decode/HeaderDecoder.hpp>
#include <exi/Decode/UnifyBuffer.hpp>
#include <exi/Grammar/Schema.hpp>
#include <exi/Stream/OrderedWriter.hpp>

namespace exi {

struct EncoderFlags {
  
};

/// The EXI decoding processor.
/// FIXME: Split this up into more implementations.
class ExiEncoder {

};

} // namespace exi
