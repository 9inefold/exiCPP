//===- Common/Twine.cpp ---------------------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Relicensed under the Apache License, Version 2.0 (the "License");
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

#include <Common/Twine.hpp>
#include <Common/SmallStr.hpp>

using namespace exi;

Str Twine::str() const {
  // If we're storing only a Str, just return it.
  if (LHSKind == StdStringKind && RHSKind == EmptyKind)
    return *LHS.stdString;

  // Otherwise, flatten and copy the contents first.
  SmallStr<256> Vec;
  return toStrRef(Vec).str();
}

void Twine::toVector(SmallVecImpl<char> &Out) const {
  raw_svector_ostream OS(Out);
  print(OS);
}

StrRef Twine::toNullTerminatedStrRef(SmallVecImpl<char> &Out) const {
  if (isUnary()) {
    switch (getLHSKind()) {
    case CStringKind:
      // Already null terminated, yay!
      return StrRef(LHS.cString);
    case StdStringKind: {
      const Str *str = LHS.stdString;
      return StrRef(str->c_str(), str->size());
    }
    case StringLiteralKind:
      return StrRef(LHS.ptrAndLength.ptr, LHS.ptrAndLength.length);
    default:
      break;
    }
  }
  this->toVector(Out);
  Out.push_back(0);
  Out.pop_back();
  return StrRef(Out.data(), Out.size());
}

void Twine::printOneChild(raw_ostream &OS, Child Ptr,
                          NodeKind Kind) const {
  switch (Kind) {
  case Twine::NullKind: break;
  case Twine::EmptyKind: break;
  case Twine::TwineKind:
    Ptr.twine->print(OS);
    break;
  case Twine::CStringKind:
    OS << Ptr.cString;
    break;
  case Twine::StdStringKind:
    OS << *Ptr.stdString;
    break;
  case Twine::PtrAndLengthKind:
  case Twine::StringLiteralKind:
    OS << StringRef(Ptr.ptrAndLength.ptr, Ptr.ptrAndLength.length);
    break;
  case Twine::CharKind:
    OS << Ptr.character;
    break;
  case Twine::DecUIKind:
    OS << Ptr.decUI;
    break;
  case Twine::DecIKind:
    OS << Ptr.decI;
    break;
  case Twine::DecULKind:
    OS << *Ptr.decUL;
    break;
  case Twine::DecLKind:
    OS << *Ptr.decL;
    break;
  case Twine::DecULLKind:
    OS << *Ptr.decULL;
    break;
  case Twine::DecLLKind:
    OS << *Ptr.decLL;
    break;
  case Twine::UHexKind:
    OS.write_hex(*Ptr.uHex);
    break;
  }
}

void Twine::printOneChildRepr(raw_ostream &OS, Child Ptr,
                              NodeKind Kind) const {
  switch (Kind) {
  case Twine::NullKind:
    OS << "null"; break;
  case Twine::EmptyKind:
    OS << "empty"; break;
  case Twine::TwineKind:
    OS << "rope:";
    Ptr.twine->printRepr(OS);
    break;
  case Twine::CStringKind:
    OS << "cstring:\""
       << Ptr.cString << "\"";
    break;
  case Twine::StdStringKind:
    OS << "std::string:\""
       << Ptr.stdString << "\"";
    break;
  case Twine::PtrAndLengthKind:
    OS << "ptrAndLength:\""
       << StringRef(Ptr.ptrAndLength.ptr, Ptr.ptrAndLength.length) << "\"";
    break;
  case Twine::StringLiteralKind:
    OS << "constexprPtrAndLength:\""
       << StringRef(Ptr.ptrAndLength.ptr, Ptr.ptrAndLength.length) << "\"";
    break;
  case Twine::CharKind:
    OS << "char:\"" << Ptr.character << "\"";
    break;
  case Twine::DecUIKind:
    OS << "decUI:\"" << Ptr.decUI << "\"";
    break;
  case Twine::DecIKind:
    OS << "decI:\"" << Ptr.decI << "\"";
    break;
  case Twine::DecULKind:
    OS << "decUL:\"" << *Ptr.decUL << "\"";
    break;
  case Twine::DecLKind:
    OS << "decL:\"" << *Ptr.decL << "\"";
    break;
  case Twine::DecULLKind:
    OS << "decULL:\"" << *Ptr.decULL << "\"";
    break;
  case Twine::DecLLKind:
    OS << "decLL:\"" << *Ptr.decLL << "\"";
    break;
  case Twine::UHexKind:
    OS << "uhex:\"" << Ptr.uHex << "\"";
    break;
  }
}

void Twine::print(raw_ostream &OS) const {
  printOneChild(OS, LHS, getLHSKind());
  printOneChild(OS, RHS, getRHSKind());
}

void Twine::printRepr(raw_ostream &OS) const {
  OS << "(Twine ";
  printOneChildRepr(OS, LHS, getLHSKind());
  OS << " ";
  printOneChildRepr(OS, RHS, getRHSKind());
  OS << ")";
}
