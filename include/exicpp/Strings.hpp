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

#include "Traits.hpp"
#include <cstring>
#include <string>
#include <fmt/format.h>

EXIP_NS_TAG namespace exip {

inline exi::StrRef format_as(const String& str) {
  return {str.str, str.length};
}

} // namespace exip

namespace exi {

std::string to_multibyte(const std::string& str);
std::string to_multibyte(const std::wstring& str);

//======================================================================//
// Objects
//======================================================================//

class String;
class ImmString;

class IString : public CString {
  template <typename T>
  static EXICPP_CXPR17 auto New(T&& t) {
    return IString::New(strdata(t), strsize(t));
  }

  static EXICPP_CXPR14 String New(Char* str, std::size_t len);
  static ImmString New(const Char* str, std::size_t len);

protected:
  StrRef asStrRef() const { return {this->str, this->length}; }
  friend StrRef format_as(IString str) { return str.asStrRef(); }
};

class String : protected IString {
public:
  String() : IString() {}
  EXICPP_CXPR14 String(Char* str, std::size_t n) : IString{str, n} {}
  EXICPP_CXPR17 String(Char* str) : String(str, strsize(str)) {}
public:
  explicit operator StrRef() const { return this->asStrRef(); }
  friend StrRef format_as(String str) { return str.asStrRef(); }
};

class ImmString : protected IString {
public:
  ImmString() : IString() {}
  ImmString(const Char* str, std::size_t len) :
   IString{const_cast<Char*>(str), len} {
  }

  ImmString(StrRef str) : ImmString(str.data(), str.size()) {}

  template <typename T>
  ALWAYS_INLINE ImmString(T&& t) :
   ImmString(strdata(t), strsize(t)) {
  }

public:
  explicit operator StrRef() const { return this->asStrRef(); }
  friend StrRef format_as(ImmString str) { return str.asStrRef(); }
};

inline EXICPP_CXPR14 String IString::New(Char* str, std::size_t len) {
  return String(str, len);
}

inline ImmString IString::New(const Char* str, std::size_t len) {
  return ImmString(str, len);
}

//////////////////////////////////////////////////////////////////////////

class QName : protected CQName {
  friend class Parser;
  friend class ContentHandler;
public:
  // TODO: Make these private in C++17.
  constexpr QName(const CQName& name) : CQName(name) {}
  QName(QName&&) = default;
private:
  constexpr QName() : CQName{} {}

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
