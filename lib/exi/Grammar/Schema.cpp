//===- exi/Grammar/Schema.cpp ---------------------------------------===//
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
/// This file defines the base for schemas.
///
//===----------------------------------------------------------------===//

#include <exi/Grammar/Schema.hpp>
#include <core/Common/MMatch.hpp>
#include <core/Common/SmallVec.hpp>
#include <exi/Stream/StreamVariant.hpp>

using namespace exi;

#ifdef __GNUC__
# define GNU_ATTR(...) __attribute__((__VA_ARGS__))
#else
# define GNU_ATTR(...)
#endif

/// The transitions for schemaless encodings are defined as the following.
/// If SC is not enabled, then the ChildContentItems for StartTagContent will
/// be (0.3) instead.
///
/// Document:
///   SD DocContent  					0
/// 
/// DocContent:
///   SE (*) DocEnd  					0
///   DT DocContent  					1.0
///   CM DocContent  					1.1.0
///   PI DocContent  					1.1.1
/// 
/// DocEnd:
///   ED            					0
///   CM DocEnd     					1.0
///   PI DocEnd     					1.1
/// 
/// StartTagContent:
///   EE  										0.0
///   AT (*) StartTagContent  0.1
///   NS StartTagContent  		0.2
///   SC Fragment  						0.3
///   ChildContentItems 		 (0.4)  
/// 
/// ElementContent:
///   EE  										0
///   ChildContentItems 		 (1.0)  
/// 
/// ChildContentItems (n.m):
///   SE (*) ElementContent  n. m
///   CH ElementContent  		 n.(m+1)
///   ER ElementContent  		 n.(m+2)
///   CM ElementContent  		 n.(m+3).0
///   PI ElementContent  		 n.(m+3).1
///

namespace {

class SCSchemaImpl {
  using enum BuiltinSchema::Grammar;
  BuiltinSchema::Grammar Current, Last;
  i64 SEDepth = 0;
public:
  SCSchemaImpl() : Current(Document), Last(Document) {}

  template <bool SelfContained>
  ALWAYS_INLINE EventTerm getTerm(StreamReader& Strm) {
    switch (Current) {
    case Document:
      this->pushGrammar(DocContent);
      return EventTerm::SD;
    case DocContent:
      tail_return this->handleDocContent(Strm);
    case DocEnd:
      tail_return this->handleDocEnd(Strm);
    case StartTagContent:
      tail_return this->handleStartTag<SelfContained>(Strm);
    case ElementContent:
      tail_return this->handleElement(Strm);
    case Fragment:
      exi_unreachable("SC elements are currently unsupported");
    }

    exi_unreachable("invalid state?");
  }

private:
  ALWAYS_INLINE void pushGrammar(BuiltinSchema::Grammar New) {
    Last = Current;
    Current = New;
  }

  EventTerm handleDocContent(StreamReader& Strm) {
    // 0
    if EXI_LIKELY(!Strm->readBit()) {
      this->pushGrammar(StartTagContent);
      ++SEDepth;
      return EventTerm::SE;
    }
    // 1.0
    if (!Strm->readBit())
      return EventTerm::DT;
    // 1.1.x
    return (!Strm->readBit())
      ? EventTerm::CM : EventTerm::PI; 
  }

  GNU_ATTR(cold) EventTerm handleDocEnd(StreamReader& Strm) {
    // 0
    if EXI_LIKELY(!Strm->readBit()) {
      exi_assert(SEDepth == 0, "invalid nesting");
      return EventTerm::ED;
    }
    // 1.x
    return (!Strm->readBit())
      ? EventTerm::CM : EventTerm::PI; 
  }

  template <bool SelfContained>
  GNU_ATTR(hot) EventTerm handleStartTag(StreamReader& Strm) {
    // 0.x
    const u64 Val = Strm->readBits<3>();
    switch (Val) {
    case 0:
      return this->handleEE(Strm);
    case 1:
      return EventTerm::AT;
    case 2:
      return EventTerm::NS;
    default:
      // 0.3?
      if (SelfContained && (Val == 3)) {
        this->pushGrammar(Fragment);
        return EventTerm::SC;
      }
      // 0.x
      static constexpr usize Off = SelfContained ? 4 : 3;
      return this->childContentItems<Off>(Strm, Val);
    }
  }

  GNU_ATTR(hot) EventTerm handleElement(StreamReader& Strm) {
    // 0
    if (!Strm->readBit())
      return this->handleEE(Strm);
    // 1.x
    const u64 Val = Strm->readBits<3>();
    return this->childContentItems<3>(Strm, Val);
  }

  template <usize M>
  ALWAYS_INLINE EventTerm childContentItems(StreamReader& Strm, const u64 Val) {
    this->pushGrammar(ElementContent);
    switch (Val) {
    case (M + 0): // n.m
      ++SEDepth;
      return EventTerm::SE;
    case (M + 1): // n.(m + 1)
      return EventTerm::CH;
    case (M + 2): // n.(m + 2)
      return EventTerm::ER;
    case (M + 3): // n.(m + 3).x
      return (!Strm->readBit())
        ? EventTerm::CM : EventTerm::PI; 
    }

    exi_unreachable("invalid child content item");
  }

  ALWAYS_INLINE EventTerm handleSE(StreamReader&) {
    this->pushGrammar(ElementContent);
    ++SEDepth;
    return EventTerm::SE;
  }

  ALWAYS_INLINE EventTerm handleEE(StreamReader&) {
    --SEDepth;
    // TODO: Check this... I'm doing everything in my power to avoid a stack.
    this->pushGrammar(SEDepth > 0 ? Last : DocEnd);
    exi_invariant(SEDepth >= 0, "invalid nesting");
    return EventTerm::EE;
  }

  template <bool SelfContained>
  ALWAYS_INLINE static bool HandleSC(const u64 Val) {
    if constexpr (SelfContained)
      return (Val == 3);
    return false;
  }
};

/// BuiltinSchema supporting SC terms.
class SCBuiltinSchema final : public BuiltinSchema, SCSchemaImpl {
  using enum BuiltinSchema::Grammar;
public:
  EventTerm getTerm(StreamReader& Strm) override {
    return SCSchemaImpl::getTerm<true>(Strm);
  }
};

/// BuiltinSchema not supporting SC terms.
class NoSCBuiltinSchema final : public BuiltinSchema, SCSchemaImpl {
  using enum BuiltinSchema::Grammar;
public:
  EventTerm getTerm(StreamReader& Strm) override {
    return SCSchemaImpl::getTerm<false>(Strm);
  }
};

} // namespace `anonymous`

Box<BuiltinSchema> BuiltinSchema::GetSchema(bool SelfContained) {
  if (SelfContained)
    return std::make_unique<SCBuiltinSchema>();
  else
    return std::make_unique<NoSCBuiltinSchema>();
}

void Schema::anchor() {}
void BuiltinSchema::anchor() {}
void DynamicSchema::anchor() {}
void CompiledSchema::anchor() {}

const char Schema::ID = 0;
const char BuiltinSchema::ID = 0;
const char DynamicSchema::ID = 0;
const char CompiledSchema::ID = 0;
