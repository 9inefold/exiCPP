//===- exi/Encode/XMLSerializer.hpp ----------------------------------===//
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
/// This file implements the interface used to encode XML as EXI.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/ArrayRef.hpp>
#include <core/Common/StrRef.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/EventCodes.hpp>
#include <exi/Encode/Serializer.hpp>

namespace exi {

class XMLSerializer final : public Serializer {
public:
  XMLSerializer(...) : Serializer() {

  }

  ExiError run(BodyEncoder* BE) override;

private:
  void anchor() override;
};

} // namespace exi
