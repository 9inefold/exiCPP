//===- exicpp/Strings.hpp -------------------------------------------===//
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

#pragma once

#ifndef EXIP_STRINGS_HPP
#define EXIP_STRINGS_HPP

#include "Basic.hpp"

namespace exi {

class QName : protected CQName {
  friend class Parser;
  constexpr QName() : CQName() {}
  constexpr QName(const CQName& name) : CQName(name) {}

  ALWAYS_INLINE static StrRef ToStr(const CString& str) {
    return StrRef(str.str, str.length);
  }

  inline static StrRef ToStr(const CString* str) {
    if EXICPP_LIKELY(str)
      return QName::ToStr(*str);
    return "";
  }

public:
  StrRef uri()        const { return QName::ToStr(CQName::uri); }
  StrRef localName()  const { return QName::ToStr(CQName::localName); }
  StrRef prefix()     const { return QName::ToStr(CQName::prefix); }
};

} // namespace exi

#endif // EXIP_STRINGS_HPP
