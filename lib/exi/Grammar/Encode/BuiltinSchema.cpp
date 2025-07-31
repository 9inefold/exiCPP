//===- exi/Grammar/Encode/BuiltinSchema.cpp -------------------------===//
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
/// This file defines the base for the builtin schema.
///
//===----------------------------------------------------------------===//

#include <exi/Grammar/EncoderSchema.hpp>
#include <core/Common/DenseMap.hpp>
#include <core/Common/EnumArray.hpp>
#include <core/Common/MMatch.hpp>
#include <core/Common/SmallVec.hpp>
#include <core/Common/STLExtras.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/Logging.hpp>
#include <core/Support/TrailingArray.hpp>
#include <exi/Basic/D/InternalMacros.hpp>
#include <exi/Basic/ExiOptions.hpp>
#include <exi/Grammar/D/BIBuilder.hpp>
#include <exi/Grammar/Grammar.hpp>
#include <exi/Stream/OrderedReader.hpp>
#include <fmt/ranges.h>
#include "SchemaGet.hpp"

using namespace exi;
using namespace exi::encode;

#define DEBUG_TYPE "BuiltinSchema"

/// Emits diagnostic for an error.
EXI_ERROR_CC static void Diagnose(const ExiError& E) {
  if (E != ExiError::OK)
    errs() << E << '\n';
}
/// Emits diagnostic for an error.
template <typename T>
EXI_ERROR_CC EXI_MINSIZE static void Diagnose(const ExiResult<T>& Result) {
  exi_invariant(Result.is_err());
  if (Result.error() != ExiError::OK)
    errs() << Result.error() << '\n';
}

//===----------------------------------------------------------------===//
// Built-in Grammar
//===----------------------------------------------------------------===//

/// The transitions for schemaless encodings are defined as the following.
/// If SC is not enabled, then the ChildContentItems for StartTagContent will
/// be (0.3) instead.
///
/// Document:
///   SD DocContent           0
/// 
/// DocContent:
///   SE (*) DocEnd           0
///   DT DocContent           1.0
///   CM DocContent           1.1.0
///   PI DocContent           1.1.1
/// 
/// DocEnd:
///   ED                      0
///   CM DocEnd               1.0
///   PI DocEnd               1.1
/// 
/// StartTagContent:
///   EE                      0.0
///   AT (*) StartTagContent  0.1
///   NS StartTagContent      0.2
///   SC Fragment             0.3
///   ChildContentItems      (0.4)  
/// 
/// ElementContent:
///   EE                      0
///   ChildContentItems      (1.0)  
/// 
/// ChildContentItems (n.m):
///   SE (*) ElementContent  n. m
///   CH ElementContent      n.(m+1)
///   ER ElementContent      n.(m+2)
///   CM ElementContent      n.(m+3).0
///   PI ElementContent      n.(m+3).1
///

namespace INTERNAL_NS(exi) {

} // namespace INTERNAL_NS

static Box<BuiltinSchema> NewChanneled(const ExiOptions& Opts) {
  exi_todo("channel readers are currently unsupported!");
}

Box<BuiltinSchema> BuiltinSchema::New(const ExiOptions& Opts) {
  switch (Opts.Alignment) {
  case AlignKind::BitPacked:
    //return OrderedBuiltinSchema<BitReader>::New(Opts);
    report_fatal_error("BitPacked currently unsupported!");
  case AlignKind::BytePacked:
    //return OrderedBuiltinSchema<ByteReader>::New(Opts);
    report_fatal_error("BytePacked currently unsupported!");
  case AlignKind::PreCompression:
    return NewChanneled(Opts);
  case AlignKind::None:
    LOG_ERROR("AlignKind cannot be None!");
    return nullptr;
  }
  exi_unreachable("invalid alignment!");
}
