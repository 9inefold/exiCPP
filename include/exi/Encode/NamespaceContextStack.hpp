//===- exi/Encode/NamespaceContextStack.hpp --------------------------===//
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
/// This file implements a stack of scopes used to save and restore namespace
/// contexts when nesting.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/ArrayRef.hpp>
#include <core/Common/Features.hpp>
#include <core/Common/Fundamental.hpp>
#include <core/Common/iterator.hpp>
#include <exi/Basic/Except.hpp>
//#include <core/Support/Limits.hpp>
// TODO: Remove StringTable include
#include <exi/Encode/StringTable.hpp>

// TODO: Do NSContextStack -> ContextPtrStack<T> and move to core/Common/?

namespace exi {

static_assert(sizeof(void*) >= 4,
  "Contact me if you need this on 16-bit machines.");

class NSContextStack;

/// Stores info about the context stack and its blocks.
template <typename Int /*SentinelValue=0*/>
struct ContextBlockInfo {
  /// Should always be >1, otherwise use the existing context.
  /// Zero is reserved for the tail scope, which should never be popped.
  Int NumElts = 0;
  /// Starts at one as it pops at zero.
  Int Depth = 1;

public:
  constexpr ContextBlockInfo() = default;
  constexpr ContextBlockInfo(Int N) : ContextBlockInfo(N, 1) {}
protected:
  friend class NSContextStack;
  constexpr ContextBlockInfo(Int N, Int Depth) : NumElts(N), Depth(Depth) {}

public:
  /// The number of elements in the current block.
  Int size() const { return NumElts; }
  /// The depth of the current block.
  Int depth() const { return Depth; }
  /// Returns whether this is the sentinel info block.
  bool isTail() const { return NumElts == 0; }
};

/// Stores info about the context stack and its blocks, as well as providing
/// some extra utility functions for accessing data.
struct ContextBlockHead : ContextBlockInfo<uhalfptr> {
  using ContextBlockInfo<uhalfptr>::ContextBlockInfo;
  using BaseT = ContextBlockInfo<uhalfptr>;
  using ElemT = encode::STPrefixEntry*;

public:
  ContextBlockHead* next() & {
    exi_invariant(BaseT::NumElts != 0);
    return reinterpret_cast<ContextBlockHead*>(
      getOffsetPtr() - (BaseT::NumElts + 1));
  }
  const ContextBlockHead* next() const& {
    return mut_self()->next();
  }

  ElemT* begin() & { return getFirstEl(); }
  ElemT* end() & { return getLastEl(); }
  const ElemT* begin() const& { return mut_self()->getFirstEl(); }
  const ElemT* end() const& { return mut_self()->getLastEl(); }

  ElemT* data() & { return getFirstEl(); }
  const ElemT* data() const& { return mut_self()->getFirstEl(); }

  MutArrayRef<ElemT> arr() & { return {begin(), end()}; }
  ArrayRef<ElemT> arr() const& { return {begin(), end()}; }

private:
  ALWAYS_INLINE ContextBlockHead* mut_self() const {
    return const_cast<ContextBlockHead*>(this);
  }
  ALWAYS_INLINE void** getOffsetPtr() {
    return reinterpret_cast<void**>(this);
  }
  ALWAYS_INLINE ElemT* getFirstEl() {
    return reinterpret_cast<ElemT*>(getOffsetPtr() - BaseT::NumElts);
  }
  ALWAYS_INLINE ElemT* getLastEl() {
    return reinterpret_cast<ElemT*>(getOffsetPtr());
  }
};

/// The blocks used by the ContextStack.
union ContextBlockEntry {
  static_assert(std::is_trivially_copyable_v<ContextBlockHead>);
  encode::STPrefixEntry* Pfx;
  ContextBlockHead Info;
public:
  constexpr ContextBlockEntry() {}
  constexpr ContextBlockEntry(encode::STPrefixEntry* Pfx) : Pfx(Pfx) {}
  constexpr ContextBlockEntry(uhalfptr NumElts) : Info(NumElts) {}

  static ContextBlockEntry Head(usize NumElts) {
    return ContextBlockEntry(IntCast<uhalfptr>(NumElts));
  }
};

class NSContextStackIterator
    : public iterator_proxy_base<NSContextStackIterator,
                                 std::forward_iterator_tag,
                                 ArrayRef<ContextBlockHead::ElemT>> {
  using Ref = iterator_facade_base::reference;
  const ContextBlockHead* Data;
public:
  explicit NSContextStackIterator(const ContextBlockHead* Data) : Data(Data) {}
  NSContextStackIterator& operator=(const NSContextStackIterator&) = default;
  iterator_facade_base::reference operator*() const { return Data->arr(); }

  inline bool operator==(const NSContextStackIterator& that) const {
    return that.Data == Data;
  }

  NSContextStackIterator& operator++() {
    this->Data = Data->next();
    return *this;
  }
};

/// Handles the saving and restoring of namespace contexts.
class NSContextStack {
public:
  /// The head of each context block.
  using BlockHead = ContextBlockHead;
  using value_type = BlockHead::ElemT;
  /// The real type stored in the stack.
  using ContextEntry = ContextBlockEntry;
  using entry_type = ContextEntry;
  /// The type returned by info functions.
  using InfoType = ContextBlockInfo<usize>;

  using iterator = NSContextStackIterator;
  using const_iterator = NSContextStackIterator;

  static_assert(sizeof(ContextEntry) == sizeof(void*));
  static_assert(std::is_trivially_copyable_v<ContextEntry>);

private:
  /// Stores the scopes. Has some inline elements to allow for an anonymous
  /// namespace to be pushed without allocation, as they are relatively common.
  SmallVec<ContextEntry, 4> Scopes;
  /// Invalidated when adding new scopes.
  BlockHead* Head = nullptr;

  /// Adds a new head block without validating the size. Should always be called
  /// after appending new scopes to maintain invariants.
  BlockHead* addHeadImpl(usize NumElts) {
    Scopes.emplace_back(ContextEntry::Head(NumElts));
    return this->getHead();
  }

  /// Gets the leading block.
  BlockHead* getHead() { return &Scopes.back().Info; }
  const BlockHead* getHead() const { return &Scopes.back().Info; }
  /// Gets the trailing block.
  BlockHead* getTail() { return &Scopes.front().Info; }
  const BlockHead* getTail() const { return &Scopes.front().Info; }

  usize incDepth() {
    return ++Head->Depth;
  }
  usize decDepth() {
    exi_invariant(Head->Depth > 0);
    return --Head->Depth;
  }

  /// Pushes the contexts to the `StringTable` and adds to a new scope.
  EXI_COLD EXI_PRESERVE_MOST void pushScope(encode::StringTable& SM,
                                            ArrayRef<value_type> Elts);
  /// Pops the contexts from the `StringTable` and removes the scope.
  EXI_COLD EXI_PRESERVE_MOST void popScope(encode::StringTable& SM);

public:
  /// New element is always added to preserve `end()` invariants.
  NSContextStack() : Head(addHeadImpl(0)) {}

  /// If empty, adds to the depth. Otherwise, pushes the contexts to the
  /// `StringTable` and adds to a new scope.
  /// @return Whether a new scope was added.
  bool push(encode::StringTable& SM, ArrayRef<value_type> Elts) {
    if EXI_LIKELY(Elts.empty()) {
      this->incDepth();
      return false;
    }
    pushScope(SM, Elts);
    return true;
  }

  /// If scope remains, subs from the depth. Otherwise, pops the contexts from
  /// the `StringTable` and removes the scope.
  void pop(encode::StringTable& SM) {
    this->decDepth();
    if EXI_UNLIKELY(Head->Depth == 0)
      popScope(SM);
  }

  iterator begin() const { return iterator(Head); }
  iterator end() const { return iterator(getTail()); }

  /// Number of elements in the current scope.
  usize size() const { return Head->NumElts; }
  /// Number of scopes deep in the current context.
  usize depth() const { return Head->Depth; }
  /// Counts the total number of scopes in the stack.
  usize total_size() const { return total().NumElts; }
  /// Counts the total number of elements in the stack.
  usize total_depth() const { return total().Depth; }
  /// Returns block info for the entire stack.
  InfoType total() const {
    InfoType Info(0, 0);
    auto* It = this->Head;

    while (!It->isTail()) {
      Info.NumElts += It->NumElts;
      Info.Depth += It->Depth;
      It = It->next();
    }

    Info.Depth += (It->Depth - 1u);
    return Info;
  }
};

} // namespace exi
