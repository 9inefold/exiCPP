//===- Detours.cpp --------------------------------------------------===//
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
/// The functions for detouring utilities.
///
//===----------------------------------------------------------------===//

#include <Detours.hpp>

using namespace re;

Jmp DetourHandler::getJmpKind() const {
  if UNLIKELY(Func == nullptr)
    return Jmp::Unknown;
  
  if (Func[0] == 0xeb)
    return Jmp::NearBYTE;
  else if (Func[0] == 0xe9)
    return Jmp::NearDWORD;
  else if (get<u16&>(0) == 0x25ff
        && get<i32&>(2) == 0)
    return Jmp::FarQWORD;
  else
    return Jmp::Unknown;
}

byte* DetourHandler::getDetouredAddress() const {
  if UNLIKELY(Func == nullptr)
    return nullptr;
  
  switch (getJmpKind()) {
  case Jmp::NearBYTE:
    return get<byte>(
      Func[1] + 2);
  case Jmp::NearDWORD:
    return get<byte>(
      get<i32&>(1) + 5);
  case Jmp::FarQWORD:
    return *get<byte*>(6);
  default:
    return nullptr;
  }
}
