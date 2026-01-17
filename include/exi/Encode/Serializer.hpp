//===- exi/Encode/Serializer.hpp -------------------------------------===//
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
/// This file implements the interface used to encode EXI.
///
//===----------------------------------------------------------------===//

#pragma once

#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Encode/BodyEncoder.hpp>

namespace exi {

/// The interface for which you can implement custom serialization.
class Serializer {
public:
  Serializer() = default;
  virtual ~Serializer() = default;
  virtual ExiError exec(BodyEncoder* BE) EXI_NONNULL(2) = 0;
  ExiError run(BodyEncoder* BE) EXI_NONNULL(2);
private:
  virtual void anchor();
};

} // namespace exi
