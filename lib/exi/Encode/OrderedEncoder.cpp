//===- exi/Encode/OrderedEncoder.cpp --------------------------------===//
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
/// This file implements an exi processor for in-order writers.
///
//===----------------------------------------------------------------===//

#include <exi/Encode/OrderedEncoder.hpp>
#include <core/Common/MMatch.hpp>
#include <core/Common/Unwrap.hpp>
#include <core/Support/Casting.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/Logging.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/ErrorCodes.hpp>

#define DEBUG_TYPE "OrderedEncoder"

using namespace exi;
using namespace exi::encode;

ExiError OrderedEncoder::run() {

}
