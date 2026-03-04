//===- Support/BinaryToText.hpp -------------------------------------===//
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
///
/// \file
/// This file defines utility classes for encoding/decoding strings and uris
/// with base64 and zbase32.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/StrRef.hpp>

namespace exi {

template <typename> class SmallVecImpl;
template <class> class Expected;

namespace base64 {
/// Encodes string to base64, returns the encoded string.
StrRef encode(StrRef Input, SmallVecImpl<char>& Buf);
/// Decodes string from base64, returns the decoded string.
Expected<StrRef> decode(StrRef Input, SmallVecImpl<char>& Buf);
} // namespace base64

namespace zbase32 {
/// Encodes string to zbase32, returns the encoded string.
StrRef encode(StrRef Input, SmallVecImpl<char>& Buf);
/// Decodes string from zbase32, returns the decoded string.
Expected<StrRef> decode(StrRef Input, SmallVecImpl<char>& Buf);
} // namespace zbase32

} // namespace exi
