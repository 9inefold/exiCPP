//===- Driver.cpp ---------------------------------------------------===//
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

#include <Common/BoxR.hpp>
#include <Common/Map.hpp>
#include <Common/String.hpp>
#include <Common/Vec.hpp>
#include <fmt/format.h>

using namespace exi;

struct Base {
  constexpr Base() : Base(false) {}
  bool isDerived() const { return this->IsDerived; }
protected:
  constexpr Base(bool V) : IsDerived(V) {}
  bool IsDerived = false;
};

struct Derived : public Base {
  constexpr Derived() : Base(true) {}
  static bool classof(const Base* B) {
    return B->isDerived();
  }
};

struct BaseV {
  constexpr BaseV() : BaseV(false) {}
  virtual ~BaseV() = default;
  bool isDerived() const { return this->IsDerived; }
protected:
  constexpr BaseV(bool V) : IsDerived(V) {}
  bool IsDerived = false;
};

struct DerivedV : public BaseV {
  constexpr DerivedV() : BaseV(true) {}
  static bool classof(const BaseV* B) {
    return B->isDerived();
  }
};

////////////////////////////////////////////////////////////////////////

bool test_null() {
  auto ptr = Box<int>::From(0);
  if (!ptr)
    return false;
  if (ptr == nullptr)
    return false;
  if (nullptr == ptr)
    return false;
  if (ptr != ptr)
    return false;
  return true;
}

bool test_isa() {
  /*Base*/ {
    auto ptr = Box<Base>::New();
    if (isa<Derived>(ptr))
      return false;
    if (auto D = dyn_cast<Derived>(ptr))
      return false;
    if (auto D = unique_dyn_cast_or_null<Derived>(ptr))
      return false;
    assert(!ptr);
  } /*Derived*/ {
    Box<BaseV> ptr = Box<DerivedV>::New();
    if (!isa<DerivedV>(ptr))
      return false;
    if (auto D = dyn_cast<DerivedV>(ptr); !D)
      return false;
    if (auto D = unique_dyn_cast_or_null<DerivedV>(ptr))
      return false;
  }

  return true;
}

int main() {
  
}
