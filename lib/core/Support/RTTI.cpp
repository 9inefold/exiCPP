//===- Support/RTTI.cpp ---------------------------------------------===//
//
// Copyright (C) 2024-2025 Eightfold
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
/// This file provides an API for getting demangled type names.
///
//===----------------------------------------------------------------===//

#include <Support/RTTI.hpp>
#include <Common/Box.hpp>
#include <Common/DenseSet.hpp>
#include <Common/SmallStr.hpp>
#include <Common/SmallVec.hpp>
#include <Config/Config.inc>
#include <Support/Allocator.hpp>
#include <Support/IntCast.hpp>
#include <Support/ManagedStatic.hpp>
#include <Support/raw_ostream.hpp>
#include <cstring>
#include <mutex>
#include <new>

// TODO: Make EXI_MULTIBUFFER_DEMANGLING a flag
#define EXI_MULTIBUFFER_DEMANGLING 1

#if EXI_USE_THREADS && !EXI_MULTIBUFFER_DEMANGLING
/// Locks on every call to a demangling function instead of once per thread.
# define USE_NAIVE_LOCKING 1
#endif

#ifndef EXI_MSVC_ENABLE_DEMANGLING
# define EXI_MSVC_ENABLE_DEMANGLING 0
#endif

#if __has_include(<cxxabi.h>)
# include <cstddef>
# include <cxxabi.h>
# define EXI_REQUIRES_DEMANGLING 1
# define CXXABI_DEMANGLE 1
#elif EXI_MSVC_ENABLE_DEMANGLING
# pragma comment(lib, "dbghelp.lib")
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# include <Windows.h>
# include <DbgHelp.h>
# include "UndName.hpp"
# define EXI_REQUIRES_DEMANGLING 1
# define UNDNAME_DEMANGLE 1
#endif

#ifndef EXI_REQUIRES_DEMANGLING
# define EXI_REQUIRES_DEMANGLING 0
#endif

#ifndef EXI_MAX_SYMBOL_LENGTH
# define EXI_MAX_SYMBOL_LENGTH (64 * 1024)
#endif // EXI_MAX_SYMBOL_LENGTH

// TODO: Add optional SymbolCache?
using namespace exi;
using namespace exi::rtti;

static StrRef AppendToBuffer(StrRef S, SmallVecImpl<char>& Buf) {
  Buf.clear();
  Buf.reserve(S.size());
  Buf.append(S.begin(), S.end());
  return StrRef(Buf.begin(), S.size());
}

static std::recursive_mutex& GetBufferPoolMtx() {
  static std::recursive_mutex BufferPoolMtx;
  return BufferPoolMtx;
}

//===----------------------------------------------------------------===//
// Interface
//===----------------------------------------------------------------===//

#if EXI_REQUIRES_DEMANGLING

/// Calls the implementation-specific demangling API.
static RttiResult<StrRef> DemangleSymbol(StrRef Symbol);

template <typename CB>
ALWAYS_INLINE static auto RttiDemangleCommon(StrRef Symbol, CB&& Callable)
 -> RttiResult<std::invoke_result_t<CB, StrRef>> {
  try {
#if USE_NAIVE_LOCKING
    std::scoped_lock Lock(GetBufferPoolMtx());
#endif
    auto Out = DemangleSymbol(Symbol);
    if (Out.is_err())
      return Err(Out.error());
    return EXI_FWD(Callable)(*Out);
  } catch (const std::bad_alloc&) {
    return Err(RttiError::InvalidMemoryAlloc);
  }
}

static RttiResult<String> RttiDemangleImpl(StrRef Symbol) {
  return RttiDemangleCommon(Symbol, [] (StrRef S) {
    return S.str();
  });
}

static RttiResult<StrRef> RttiDemangleImpl(
 StrRef Symbol, SmallVecImpl<char>& Buf) {
  return RttiDemangleCommon(Symbol, [&Buf] (StrRef S) {
    return AppendToBuffer(S, Buf);
  });
}

static RttiError RttiDemangleImpl(
 StrRef Symbol, raw_ostream& OS) {
  return RttiDemangleCommon(Symbol, [&OS] (StrRef S) {
    OS.write(S.data(), S.size());
    return 0;
  }).error_or(RttiError::Success);
}

#else
static ALWAYS_INLINE RttiResult<String> RttiDemangleImpl(StrRef Symbol) {
  return String(Symbol.data(), Symbol.size());
}

static ALWAYS_INLINE RttiResult<StrRef> RttiDemangleImpl(
 StrRef Symbol, SmallVecImpl<char>& Buf) {
  return AppendToBuffer(Symbol, Buf);
}
#endif

//===----------------------------------------------------------------===//
// Exposed API
//===----------------------------------------------------------------===//

static Option<RttiError> DemangleChk(const char* Symbol) {
  if EXI_UNLIKELY(!Symbol)
    return RttiError::InvalidArgument;
  else if EXI_UNLIKELY(Symbol[0] == '\0')
    return RttiError::InvalidName;
  else
    return std::nullopt;
}

static Option<RttiError> DemangleChk(StrRef Symbol) {
  if EXI_UNLIKELY(Symbol.empty())
    return RttiError::InvalidName;
  else
    return std::nullopt;
}

//////////////////////////////////////////////////////////////////////////
// Empty

RttiResult<String> exi::rtti::demangle(const char* Symbol) {
  if (auto OptErr = DemangleChk(Symbol))
    return Err(*OptErr);
  else
    return RttiDemangleImpl(StrRef(Symbol));
}

RttiResult<String> exi::rtti::demangle(StrRef Symbol) {
  if (auto OptErr = DemangleChk(Symbol))
    return Err(*OptErr);
  return RttiDemangleImpl(Symbol);
}

#if !EXI_COMPILER(MSVC)
RttiResult<String> exi::rtti::demangle(const std::type_info& Info) {
  return RttiDemangleImpl(StrRef(Info.name()));
}
#endif // !MSVC

//////////////////////////////////////////////////////////////////////////
// Buffered

RttiResult<StrRef> exi::rtti::demangle(
 const char* Symbol, SmallVecImpl<char>& Buf) {
  if (auto OptErr = DemangleChk(Symbol))
    return Err(*OptErr);
  else
    return RttiDemangleImpl(StrRef(Symbol), Buf);
}

RttiResult<StrRef> exi::rtti::demangle(
 StrRef Symbol, SmallVecImpl<char>& Buf) {
  if (auto OptErr = DemangleChk(Symbol))
    return Err(*OptErr);
  tail_return RttiDemangleImpl(Symbol, Buf);
}

RttiResult<StrRef> exi::rtti::demangle(
 const std::type_info& Info, SmallVecImpl<char>& Buf) {
  return RttiDemangleImpl(StrRef(Info.name()), Buf);
}

//////////////////////////////////////////////////////////////////////////
// raw_ostream

RttiError exi::rtti::demangle(const char* Symbol, raw_ostream& OS) {
  if (auto OptErr = DemangleChk(Symbol))
    return *OptErr;
  else
    return RttiDemangleImpl(StrRef(Symbol), OS);
}

RttiError exi::rtti::demangle(StrRef Symbol, raw_ostream& OS) {
  if (auto OptErr = DemangleChk(Symbol))
    return *OptErr;
  tail_return RttiDemangleImpl(Symbol, OS);
}

RttiError exi::rtti::demangle(
 const std::type_info& Info, raw_ostream& OS) {
  return RttiDemangleImpl(StrRef(Info.name()), OS);
}

//===----------------------------------------------------------------===//
// Implementation
//===----------------------------------------------------------------===//

#if EXI_REQUIRES_DEMANGLING

namespace {

//////////////////////////////////////////////////////////////////////////
// DemanglerBuffer

struct DemanglerBufferDeleter {
  void operator()(char* Ptr) const {
    if EXI_UNLIKELY(!Ptr)
      return;
    std::free(Ptr);
  }
};

class DemanglerBuffer {
  /// The type used to store the buffer.
  using StorageType = Box<char[], DemanglerBufferDeleter>;
  /// The actual buffer.
  StorageType Storage;

  EXI_PREFER_TYPE(bool)
  /// Whether the buffer has been resized.
  usize HasResized : 1;

  /// The estimated size of the buffer.
  usize Size : sizeof(usize) - 1;

  /// Allocates the initial character buffer.
  static char* AllocateBuffer(usize Size);

  void setSize(usize NewSize) {
    // FIXME: Make warning?
    exi_invariant(NewSize > Size);
    this->HasResized = true;
    this->Size = NewSize;
  }

  bool isPtrInRange(const char* Ptr) const {
    const char* Begin = Storage.get();
    const char* End = Begin + Size;
    return (Ptr >= Begin) && (Ptr < End);
  }

public:
  DemanglerBuffer() : DemanglerBuffer(EXI_MAX_SYMBOL_LENGTH) {}
  DemanglerBuffer(usize Size) :
   Storage(AllocateBuffer(Size)),
   HasResized(false), Size(Size) {
  }

  std::pair<char*, usize> bufAndSize() const {
    return {Storage.get(), Size};
  }

  void reallocate(usize NewSize) {
    if (NewSize <= Size)
      return;
    char* NewPtr = AllocateBuffer(NewSize);
    Storage.reset(NewPtr);
    this->setSize(NewSize);
  }

  void maybeReplace(char* NewPtr, usize NewSize);

  bool hasResized() const {
    return this->HasResized;
  }
  usize size() const {
    return this->Size;
  }
};

char* DemanglerBuffer::AllocateBuffer(usize Size) {
  // FIXME: This can actually be done with mimalloc on MSVC.
  void* Ptr = std::malloc(Size);
  if EXI_UNLIKELY(!Ptr) {
    if (Size == 0)
      return AllocateBuffer(1);
    fatal_alloc_error("Allocation failed");
  }
  return static_cast<char*>(Ptr);
}

void DemanglerBuffer::maybeReplace(char* NewPtr, usize NewSize) {
  if EXI_UNLIKELY(!NewPtr)
    return;
  
  StorageType LocalStore(NewPtr);
  if EXI_LIKELY(isPtrInRange(NewPtr)) {
    // Release to avoid double frees.
    (void) LocalStore.release();
    if EXI_LIKELY(NewSize <= Size)
      // This is the most likely path as our buffers are huge.
      return;
    // The buffer has been reallocated, resize.
    this->setSize(NewSize);
    return;
  }

  if EXI_UNLIKELY(NewSize <= Size) {
    // This should basically never be reached, as storage is generally only
    // obtained when reallocating. The condition is checked either way.
    return;
  }

  // Swap to deallocate the old storage.
  std::swap(this->Storage, LocalStore);
  this->setSize(NewSize);
}

//////////////////////////////////////////////////////////////////////////
// DemanglerBufferPool

class DemanglerBufferPool;

#if !EXI_USE_THREADS || !EXI_MULTIBUFFER_DEMANGLING

/// Simple single threaded "pool".
class DemanglerBufferPool {
  DemanglerBuffer Buffer;
public:
  DemanglerBufferPool() = default;
  DemanglerBuffer* claimBuffer() { return &Buffer; }
};

#else

class DemanglerBufferHandle {
  friend class DemanglerBufferPool;
  DemanglerBufferPool* Pool = nullptr;
  DemanglerBuffer* Buffer = nullptr;

  DemanglerBufferHandle(DemanglerBufferPool* Pool, 
                        DemanglerBuffer* Buf) : Pool(Pool), Buffer(Buf) {}
public:
  inline ~DemanglerBufferHandle();
  ALWAYS_INLINE DemanglerBuffer* get() const { return Buffer; }
};

/// Thread-safe buffer manager with reusable handles.
class DemanglerBufferPool {
  /// Allocator to unique Buffers.
  SpecificBumpPtrAllocator<DemanglerBuffer> Alloc;
  /// The set of active Buffers.
  SmallDenseSet<DemanglerBuffer*, 8> ActiveList;
  /// The list of available Buffers.
  SmallVec<DemanglerBuffer*, 1> FreeList;

  /// Returns a unique handle for the current thread.
  DemanglerBufferHandle getHandle();
  std::pair<DemanglerBuffer*, bool> claimBufferLocked();

public:
  DemanglerBufferPool() = default;
  DemanglerBuffer* claimBuffer();
  void freeBuffer(DemanglerBufferHandle* Hold);
};

std::pair<DemanglerBuffer*, bool> DemanglerBufferPool::claimBufferLocked() {
  std::scoped_lock Lock(GetBufferPoolMtx());
  if (!FreeList.empty()) {
    DemanglerBuffer* Buf = FreeList.pop_back_val();
    const bool DidInsert = ActiveList.insert(Buf).second;
    return {Buf, DidInsert};
  }

  DemanglerBuffer* Buf = Alloc.Allocate(1);
  const bool DidInsert = ActiveList.insert(Buf).second;
  exi_assert(DidInsert, "Invalid program state (reused storage)");
  return {Buf, DidInsert};
}

DemanglerBufferHandle DemanglerBufferPool::getHandle() {
  auto [Buf, DidInsert] = this->claimBufferLocked();
  exi_relassert(DidInsert, "Invalid pool state, item in "
                           "FreeList was found in ActiveList");
  return DemanglerBufferHandle(this, Buf);
}

/// Claims buffer for the current thread.
DemanglerBuffer* DemanglerBufferPool::claimBuffer() {
  static thread_local DemanglerBufferHandle
    ThreadLocalHandle = this->getHandle();
  return ThreadLocalHandle.get();
}

void DemanglerBufferPool::freeBuffer(DemanglerBufferHandle* Hold) {
  exi_relassert(Hold != nullptr);
  DemanglerBuffer* Buf = Hold->get();
  bool DidErase = true;

  {
    std::scoped_lock Lock(GetBufferPoolMtx());
    const bool DidErase = ActiveList.erase(Buf);
    FreeList.emplace_back(Buf);
  }

  exi_invariant(DidErase);
}

DemanglerBufferHandle::~DemanglerBufferHandle() {
  if (!Pool) return;
  Pool->freeBuffer(this);
}

#endif

} // namespace `anonymous`

static ManagedStatic<DemanglerBufferPool> Pool;

//////////////////////////////////////////////////////////////////////////
// Demangling

# if CXXABI_DEMANGLE

/// Returns the data or status code.
static Result<StrRef, int> Invoke__cxa_demangle(const char* Symbol) {
  DemanglerBuffer* Buf = Pool->claimBuffer();
  exi_invariant(Buf != nullptr, "DemanglerBuffer allocation failed.");

  int Status = 0;
  auto [Data, Len] = Buf->bufAndSize();
  char* Out = abi::__cxa_demangle(Symbol, Data, &Len, &Status);

  if (Out == nullptr)
    return Err(Status);
  
  Buf->maybeReplace(Out, Len);
  return StrRef(Out, Len);
}

static RttiError MapDemangleStatus(int Status) {
  if EXI_LIKELY(Status <= 0 && Status >= -3)
    return static_cast<RttiError>(Status);
  return RttiError::Other;
}

static RttiResult<StrRef> DemangleSymbol(StrRef Symbol) {
  Result Out = Invoke__cxa_demangle(Symbol.data());
  if (Out.is_err()) {
    const int Status = Out.error();
    return Err(MapDemangleStatus(Status));
  }
  return Ok(*Out);
}

# elif UNDNAME_DEMANGLE

ALWAYS_INLINE static char* Bridge__unDName(
 const char* Symbol, char* Buffer, usize Size, UndStrategy::Type Flags) {
  const int ChkSize = exi::IntCastOrZero<int>(Size);
  return __unDName(Buffer, Symbol, ChkSize, &std::malloc, &std::free, Flags);
}

// TODO: Make this better? Might need a method of reloading symbols
[[nodiscard]] EXI_NO_INLINE static int DemangleSymInit() {
  SymSetOptions(SYMOPT_ALLOW_ABSOLUTE_SYMBOLS | SYMOPT_DEFERRED_LOADS);
  bool DidInit = !!SymInitialize(GetCurrentProcess(), nullptr, TRUE);
  assert(did_initialize && "Failed to initialize the symbol handler!");
  return true;
}

// TODO: Implement undname demangling, adapt LLVM impl

static RttiResult<StrRef> DemangleSymbol(StrRef Symbol) {
  { [[maybe_unused]] static volatile int LazyLoad = DemangleSymInit(); }
  bool IsInternal = Symbol.consume_front('.');
  exi_unreachable("implement undname DemangleSymbol");
}

# else
#  error Unknown demangling API!
# endif

#endif // EXI_REQUIRES_DEMANGLING
