//===- exi/Encode/XMLSerializer.cpp ----------------------------------===//
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

#include <exi/Encode/XMLSerializer.hpp>
#include <core/Support/Casting.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/D/InternalMacros.hpp>
//#include <exi/Encode/ChannelEncoder.hpp>
#include <exi/Encode/OrderedEncoder.hpp>

using namespace exi;
using namespace exi::encode;

#define DEBUG_TYPE "XMLSerializer"

namespace INTERNAL_NS(exi) {

class INTERNAL_LINKAGE GenericEncoderRunner {
protected:
  XMLDocument& Doc;
public:
  GenericEncoderRunner(XMLDocument& Doc) : Doc(Doc) {}
  ExiError run() { return ExiError::TODO; }
};

template <class Encoder>
class INTERNAL_LINKAGE EncoderRunner : public GenericEncoderRunner {
  friend class GenericEncoderRunner;
  Encoder& BE;
public:
  EncoderRunner(XMLDocument& Doc, Encoder& BE)
   : GenericEncoderRunner(Doc), BE(BE) {}
};

} // namespace INTERNAL_NS

//////////////////////////////////////////////////////////////////////////
// Interface

template <class Encoder>
static ExiError RunAs(XMLDocument& Doc, BodyEncoder* BE) {
  auto& EE = cast<Encoder>(*BE);
  EncoderRunner<Encoder> ER(Doc, EE);
  return ER.run();
}

static ExiError Run(XMLDocument& Doc, BodyEncoder* BE) {
  if EXI_NEVER(!isa<OrderedEncoder>(*BE)) {
    LOG_ERROR("Non-ordered encoders have not been implemented!");
    return ExiError::TODO;
  }
  tail_return RunAs<OrderedEncoder>(Doc, BE);
}

//===----------------------------------------------------------------===//
// [Owning]XMLSerializer
//===----------------------------------------------------------------===//

ExiError XMLSerializer::run(BodyEncoder* BE) {
  if EXI_NEVER(Doc == nullptr) {
    LOG_ERROR("Null XML document!");
    return ErrorCode::kNullptrRef;
  }
  return Run(*Doc, BE);
}

ExiError OwningXMLSerializer::run(BodyEncoder* BE) {
  exi_todo("add parsing interface");
  return Run(Doc, BE);
}
