//===- Support/ScopedSave.hpp ----------------------------------------===//
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
///
/// \file
/// This file provides utility classes that use RAII to save and restore
/// values.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <utility>
#include <atomic>

namespace exi {
namespace H {

template <typename T>
struct ScopedSaveType { using type = T; };

template <typename T>
struct ScopedSaveType<std::atomic<T>> { using type = T; };

template <typename T>
struct ScopedSaveType<volatile std::atomic<T>> { using type = T; };

} // namespace H

template <typename T>
using scoped_save_t = typename H::ScopedSaveType<T>::type;

/// A utility class that uses RAII to save and restore the value of a variable.
template <typename T> struct ScopedSave {
  using Type = scoped_save_t<T>;
  ScopedSave(T& X) : X(X), OldValue(X) {}
  ScopedSave(T& X, const Type& NewValue) : X(X), OldValue(X) { X = NewValue; }
  ScopedSave(T& X, Type&& NewValue) : X(X), OldValue(std::move(X)) {
    X = std::move(NewValue);
  }
  ~ScopedSave() { X = std::move(OldValue); }
  const Type& get() { return OldValue; }

private:
  T& X;
  Type OldValue;
};

// User-defined CTAD guides.
template <typename T> ScopedSave(T&) -> ScopedSave<T>;
template <typename T> ScopedSave(T&, const scoped_save_t<T>&) -> ScopedSave<T>;
template <typename T> ScopedSave(T&, scoped_save_t<T>&&) -> ScopedSave<T>;

} // namespace exi
