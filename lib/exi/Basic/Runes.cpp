//===- exi/Basic/Runes.cpp ------------------------------------------===//
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
/// This file defines an interface for unicode codepoints (runes).
///
//===----------------------------------------------------------------===//

#include <exi/Basic/Runes.hpp>
#include <core/Common/ArrayRef.hpp>
#include <core/Common/SmallVec.hpp>

using namespace exi;

bool exi::decodeRunes(RuneDecoder Decoder,
											SmallVecImpl<Rune>& Runes) {
	bool Result = true;
	while (Decoder) {
		const Rune Decoded = Decoder.decode();
		if EXI_UNLIKELY(Decoded == kInvalidRune)
			Result = false;
		Runes.push_back(Decoded);
	}
	return Result;
}

bool exi::decodeRunesUnchecked(RuneDecoder Decoder,
															 SmallVecImpl<Rune>& Runes) {
  while (Decoder) {
		Runes.push_back(
			Decoder.decodeUnchecked());
	}
	return true;
}
