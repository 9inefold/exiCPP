//===- Support/MemoryBuffer.hpp --------------------------------------===//
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
//
//  This file defines the MemoryBuffer interface.
//
//===----------------------------------------------------------------===//

#include <Support/MemoryBuffer.hpp>
#include <Common/STLExtras.hpp>
#include <Common/SmallStr.hpp>
#include <Config/Config.inc>
#include <Support/D/IO.hpp>
#include <Support/Alignment.hpp>
#include <Support/Errc.hpp>
#include <Support/Error.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/Filesystem.hpp>
#include <Support/Process.hpp>
#include <Support/Program.hpp>
#include <Support/Alloc.hpp>
#include <Support/SmallVecMemoryBuffer.hpp>
#include <algorithm>
#include <cstring>
#include <new>
#include <sys/types.h>
#include <system_error>

#ifdef __MVS__
# include <Support/AutoConvert.hpp>
#endif

GCC_IGNORED("-Wredundant-move")

using namespace exi;

//===----------------------------------------------------------------------===//
// MemoryBuffer implementation itself.
//===----------------------------------------------------------------------===//

MemoryBuffer::~MemoryBuffer() = default;

/// init - Initialize this MemoryBuffer as a reference to externally allocated
/// memory, memory that we know is already null terminated.
void MemoryBuffer::init(const char *BufStart, const char *BufEnd,
                        bool RequiresNullTerminator) {
  exi_assert((!RequiresNullTerminator || BufEnd[0] == 0),
         "Buffer is not null terminated!");
  BufferStart = BufStart;
  BufferEnd = BufEnd;
}

//===----------------------------------------------------------------------===//
// MemoryBufferMem implementation.
//===----------------------------------------------------------------------===//

/// CopyStrRef - Copies contents of a StrRef into a block of memory and
/// null-terminates it.
static void CopyStrRef(char *Memory, StrRef Data) {
  if (!Data.empty())
    memcpy(Memory, Data.data(), Data.size());
  Memory[Data.size()] = 0; // Null terminate string.
}

namespace {
struct NamedBufferAlloc {
  const Twine &Name;
  NamedBufferAlloc(const Twine &Name) : Name(Name) {}
};
} // namespace

void *operator new(usize N, const NamedBufferAlloc &Alloc) {
  SmallStr<256> NameBuf;
  StrRef NameRef = Alloc.Name.toStrRef(NameBuf);

  // We use malloc() and manually handle it returning null instead of calling
  // operator new because we need all uses of NamedBufferAlloc to be
  // deallocated with a call to free() due to needing to use malloc() in
  // WritableMemoryBuffer::getNewUninitMemBuffer() to work around the out-of-
  // memory handler installed by default in LLVM. See operator delete() member
  // functions within this file for the paired call to free().
  char *Mem =
      static_cast<char *>(
        exi::exi_malloc(N + sizeof(usize) + NameRef.size() + 1));
  if (!Mem)
    exi::fatal_alloc_error("Allocation failed");
  *reinterpret_cast<usize *>(Mem + N) = NameRef.size();
  CopyStrRef(Mem + N + sizeof(usize), NameRef);
  return Mem;
}

namespace {
/// MemoryBufferMem - Named MemoryBuffer pointing to a block of memory.
template<typename MB>
class MemoryBufferMem : public MB {
public:
  MemoryBufferMem(StrRef InputData, bool RequiresNullTerminator) {
    MemoryBuffer::init(InputData.begin(), InputData.end(),
                       RequiresNullTerminator);
  }

  /// Disable sized deallocation for MemoryBufferMem, because it has
  /// tail-allocated data.
  void operator delete(void *p) { exi::exi_free(p); }

  StrRef getBufferIdentifier() const override {
    // The name is stored after the class itself.
    return StrRef(reinterpret_cast<const char *>(this + 1) + sizeof(usize),
                     *reinterpret_cast<const usize *>(this + 1));
  }

  MemoryBuffer::BufferKind getBufferKind() const override {
    return MemoryBuffer::MemoryBuffer_Malloc;
  }
};
} // namespace

template <typename MB>
static ErrorOr<Box<MB>>
getFileAux(const Twine &Filename, u64 MapSize, u64 Offset,
           bool IsText, bool RequiresNullTerminator, bool IsVolatile,
           Option<Align> Alignment);

Box<MemoryBuffer>
MemoryBuffer::getMemBuffer(StrRef InputData, StrRef BufferName,
                           bool RequiresNullTerminator) {
  auto *Ret = new (NamedBufferAlloc(BufferName))
      MemoryBufferMem<MemoryBuffer>(InputData, RequiresNullTerminator);
  return Box<MemoryBuffer>(Ret);
}

Box<MemoryBuffer>
MemoryBuffer::getMemBuffer(MemoryBufferRef Ref, bool RequiresNullTerminator) {
  return Box<MemoryBuffer>(getMemBuffer(
      Ref.getBuffer(), Ref.getBufferIdentifier(), RequiresNullTerminator));
}

static ErrorOr<Box<WritableMemoryBuffer>>
getMemBufferCopyImpl(StrRef InputData, const Twine &BufferName) {
  auto Buf =
      WritableMemoryBuffer::getNewUninitMemBuffer(InputData.size(), BufferName);
  if (!Buf)
    return make_error_code(errc::not_enough_memory);
  // Calling memcpy with null src/dst is UB, and an empty StrRef is
  // represented with {nullptr, 0}.
  exi::copy(InputData, Buf->getBufferStart());
  return std::move(Buf);
}

Box<MemoryBuffer>
MemoryBuffer::getMemBufferCopy(StrRef InputData, const Twine &BufferName) {
  auto Buf = getMemBufferCopyImpl(InputData, BufferName);
  if (Buf)
    return std::move(*Buf);
  return nullptr;
}

ErrorOr<Box<MemoryBuffer>>
MemoryBuffer::getFileOrSTDIN(const Twine &Filename, bool IsText,
                             bool RequiresNullTerminator,
                             Option<Align> Alignment) {
  SmallStr<256> NameBuf;
  StrRef NameRef = Filename.toStrRef(NameBuf);

  if (NameRef == "-")
    return getSTDIN();
  return getFile(Filename, IsText, RequiresNullTerminator,
                 /*IsVolatile=*/false, Alignment);
}

ErrorOr<Box<MemoryBuffer>>
MemoryBuffer::getFileSlice(const Twine &FilePath, u64 MapSize,
                           u64 Offset, bool IsVolatile,
                           Option<Align> Alignment) {
  return getFileAux<MemoryBuffer>(FilePath, MapSize, Offset, /*IsText=*/false,
                                  /*RequiresNullTerminator=*/false, IsVolatile,
                                  Alignment);
}

//===----------------------------------------------------------------------===//
// MemoryBuffer::getFile implementation.
//===----------------------------------------------------------------------===//

namespace {

template <typename MB>
constexpr sys::fs::mapped_file_region::mapmode Mapmode =
    sys::fs::mapped_file_region::readonly;
template <>
constexpr sys::fs::mapped_file_region::mapmode Mapmode<MemoryBuffer> =
    sys::fs::mapped_file_region::readonly;
template <>
constexpr sys::fs::mapped_file_region::mapmode Mapmode<WritableMemoryBuffer> =
    sys::fs::mapped_file_region::priv;
template <>
constexpr sys::fs::mapped_file_region::mapmode
    Mapmode<WriteThroughMemoryBuffer> = sys::fs::mapped_file_region::readwrite;

/// Memory maps a file descriptor using sys::fs::mapped_file_region.
///
/// This handles converting the offset into a legal offset on the platform.
template<typename MB>
class MemoryBufferMMapFile : public MB {
  sys::fs::mapped_file_region MFR;

  static u64 getLegalMapOffset(u64 Offset) {
    return Offset & ~(sys::fs::mapped_file_region::alignment() - 1);
  }

  static u64 getLegalMapSize(u64 Len, u64 Offset) {
    return Len + (Offset - getLegalMapOffset(Offset));
  }

  const char *getStart(u64 Len, u64 Offset) {
    return MFR.const_data() + (Offset - getLegalMapOffset(Offset));
  }

public:
  MemoryBufferMMapFile(bool RequiresNullTerminator, sys::fs::file_t FD, u64 Len,
                       u64 Offset, std::error_code &EC)
      : MFR(FD, Mapmode<MB>, getLegalMapSize(Len, Offset),
            getLegalMapOffset(Offset), EC) {
    if (!EC) {
      const char *Start = getStart(Len, Offset);
      MemoryBuffer::init(Start, Start + Len, RequiresNullTerminator);
    }
  }

  /// Disable sized deallocation for MemoryBufferMMapFile, because it has
  /// tail-allocated data.
  void operator delete(void *p) { exi::exi_free(p); }

  StrRef getBufferIdentifier() const override {
    // The name is stored after the class itself.
    return StrRef(reinterpret_cast<const char *>(this + 1) + sizeof(usize),
                     *reinterpret_cast<const usize *>(this + 1));
  }

  MemoryBuffer::BufferKind getBufferKind() const override {
    return MemoryBuffer::MemoryBuffer_MMap;
  }

  void dontNeedIfMmap() override { MFR.dontNeed(); }
};
} // namespace

static ErrorOr<Box<WritableMemoryBuffer>>
getMemoryBufferForStream(sys::fs::file_t FD, const Twine &BufferName) {
  SmallStr<sys::fs::DefaultReadChunkSize> Buffer;
  if (Error E = sys::fs::readNativeFileToEOF(FD, Buffer))
    return errorToErrorCode(std::move(E));
  return getMemBufferCopyImpl(Buffer, BufferName);
}

ErrorOr<Box<MemoryBuffer>>
MemoryBuffer::getFile(const Twine &Filename, bool IsText,
                      bool RequiresNullTerminator, bool IsVolatile,
                      Option<Align> Alignment) {
  return getFileAux<MemoryBuffer>(Filename, /*MapSize=*/-1, /*Offset=*/0,
                                  IsText, RequiresNullTerminator, IsVolatile,
                                  Alignment);
}

template <typename MB>
static ErrorOr<Box<MB>>
getOpenFileImpl(sys::fs::file_t FD, const Twine &Filename, u64 FileSize,
                u64 MapSize, i64 Offset, bool RequiresNullTerminator,
                bool IsVolatile, Option<Align> Alignment);

template <typename MB>
static ErrorOr<Box<MB>>
getFileAux(const Twine &Filename, u64 MapSize, u64 Offset,
           bool IsText, bool RequiresNullTerminator, bool IsVolatile,
           Option<Align> Alignment) {
  Expected<sys::fs::file_t> FDOrErr = sys::fs::openNativeFileForRead(
      Filename, IsText ? sys::fs::OF_TextWithCRLF : sys::fs::OF_None);
  if (!FDOrErr)
    return errorToErrorCode(FDOrErr.takeError());
  sys::fs::file_t FD = *FDOrErr;
  auto Ret = getOpenFileImpl<MB>(FD, Filename, /*FileSize=*/-1, MapSize, Offset,
                                 RequiresNullTerminator, IsVolatile, Alignment);
  sys::fs::closeFile(FD);
  return Ret;
}

ErrorOr<Box<WritableMemoryBuffer>>
WritableMemoryBuffer::getFile(const Twine &Filename, bool IsVolatile,
                              Option<Align> Alignment) {
  return getFileAux<WritableMemoryBuffer>(
      Filename, /*MapSize=*/-1, /*Offset=*/0, /*IsText=*/false,
      /*RequiresNullTerminator=*/false, IsVolatile, Alignment);
}

ErrorOr<Box<WritableMemoryBuffer>>
WritableMemoryBuffer::getFileEx(const Twine &Filename,
                                bool RequiresNullTerminator, bool IsVolatile,
                                Option<Align> Alignment) {
  return getFileAux<WritableMemoryBuffer>(
      Filename, /*MapSize=*/-1, /*Offset=*/0, /*IsText=*/false,
      RequiresNullTerminator, IsVolatile, Alignment);
}

ErrorOr<Box<WritableMemoryBuffer>>
WritableMemoryBuffer::getFileSlice(const Twine &Filename, u64 MapSize,
                                   u64 Offset, bool IsVolatile,
                                   Option<Align> Alignment) {
  return getFileAux<WritableMemoryBuffer>(
      Filename, MapSize, Offset, /*IsText=*/false,
      /*RequiresNullTerminator=*/false, IsVolatile, Alignment);
}

Box<WritableMemoryBuffer>
WritableMemoryBuffer::getNewUninitMemBuffer(usize Size,
                                            const Twine &BufferName,
                                            Option<Align> Alignment) {
  using MemBuffer = MemoryBufferMem<WritableMemoryBuffer>;

  // Use 16-byte alignment if no alignment is specified.
  Align BufAlign = Alignment.value_or(Align(16));

  // Allocate space for the MemoryBuffer, the data and the name. It is important
  // that MemoryBuffer and data are aligned so PointerIntPair works with them.
  SmallStr<256> NameBuf;
  StrRef NameRef = BufferName.toStrRef(NameBuf);

  usize StringLen = sizeof(MemBuffer) + sizeof(usize) + NameRef.size() + 1;
  usize RealLen = StringLen + Size + 1 + BufAlign.value();
  if (RealLen <= Size) // Check for rollover.
    return nullptr;
  // We use a call to malloc() rather than a call to a non-throwing operator
  // new() because LLVM unconditionally installs an out of memory new handler
  // when exceptions are disabled. This new handler intentionally crashes to
  // aid with debugging, but that makes non-throwing new calls unhelpful.
  // See MemoryBufferMem::operator delete() for the paired call to free(), and
  // exi::install_out_of_memory_new_handler() for the installation of the
  // custom new handler.
  char *Mem = static_cast<char *>(exi::exi_malloc(RealLen));
  if (!Mem)
    return nullptr;

  // The name is stored after the class itself.
  *reinterpret_cast<usize *>(Mem + sizeof(MemBuffer)) = NameRef.size();
  CopyStrRef(Mem + sizeof(MemBuffer) + sizeof(usize), NameRef);

  // The buffer begins after the name and must be aligned.
  char *Buf = (char *)alignAddr(Mem + StringLen, BufAlign);
  Buf[Size] = 0; // Null terminate buffer.

  auto *Ret = new (Mem) MemBuffer(StrRef(Buf, Size), true);
  return Box<WritableMemoryBuffer>(Ret);
}

Box<WritableMemoryBuffer>
WritableMemoryBuffer::getNewMemBuffer(usize Size, const Twine &BufferName) {
  auto SB = WritableMemoryBuffer::getNewUninitMemBuffer(Size, BufferName);
  if (!SB)
    return nullptr;
  memset(SB->getBufferStart(), 0, Size);
  return SB;
}

static bool shouldUseMmap(sys::fs::file_t FD,
                          usize FileSize,
                          usize MapSize,
                          off_t Offset,
                          bool RequiresNullTerminator,
                          int PageSize,
                          bool IsVolatile) {
#if defined(__MVS__)
  // zOS Enhanced ASCII auto convert does not support mmap.
  return false;
#endif

  // mmap may leave the buffer without null terminator if the file size changed
  // by the time the last page is mapped in, so avoid it if the file size is
  // likely to change.
  if (IsVolatile && RequiresNullTerminator)
    return false;

  // We don't use mmap for small files because this can severely fragment our
  // address space.
  if (MapSize < 4 * 4096 || MapSize < (unsigned)PageSize)
    return false;

  if (!RequiresNullTerminator)
    return true;

  // If we don't know the file size, use fstat to find out.  fstat on an open
  // file descriptor is cheaper than stat on a random path.
  // FIXME: this chunk of code is duplicated, but it avoids a fstat when
  // RequiresNullTerminator = false and MapSize != -1.
  if (FileSize == usize(-1)) {
    sys::fs::file_status Status;
    if (sys::fs::status(FD, Status))
      return false;
    FileSize = Status.getSize();
  }

  // If we need a null terminator and the end of the map is inside the file,
  // we cannot use mmap.
  usize End = Offset + MapSize;
  assert(End <= FileSize);
  if (End != FileSize)
    return false;

  // Don't try to map files that are exactly a multiple of the system page size
  // if we need a null terminator.
  if ((FileSize & (PageSize -1)) == 0)
    return false;

#if defined(__CYGWIN__)
  // Don't try to map files that are exactly a multiple of the physical page size
  // if we need a null terminator.
  // FIXME: We should reorganize again getPageSize() on Win32.
  if ((FileSize & (4096 - 1)) == 0)
    return false;
#endif

  return true;
}

static ErrorOr<Box<WriteThroughMemoryBuffer>>
getReadWriteFile(const Twine &Filename, u64 FileSize, u64 MapSize,
                 u64 Offset) {
  Expected<sys::fs::file_t> FDOrErr = sys::fs::openNativeFileForReadWrite(
      Filename, sys::fs::CD_OpenExisting, sys::fs::OF_None);
  if (!FDOrErr)
    return errorToErrorCode(FDOrErr.takeError());
  sys::fs::file_t FD = *FDOrErr;

  // Default is to map the full file.
  if (MapSize == u64(-1)) {
    // If we don't know the file size, use fstat to find out.  fstat on an open
    // file descriptor is cheaper than stat on a random path.
    if (FileSize == u64(-1)) {
      sys::fs::file_status Status;
      std::error_code EC = sys::fs::status(FD, Status);
      if (EC)
        return EC;

      // If this not a file or a block device (e.g. it's a named pipe
      // or character device), we can't mmap it, so error out.
      sys::fs::file_type Type = Status.type();
      if (Type != sys::fs::file_type::regular_file &&
          Type != sys::fs::file_type::block_file)
        return make_error_code(errc::invalid_argument);

      FileSize = Status.getSize();
    }
    MapSize = FileSize;
  }

  std::error_code EC;
  Box<WriteThroughMemoryBuffer> Result(
      new (NamedBufferAlloc(Filename))
          MemoryBufferMMapFile<WriteThroughMemoryBuffer>(false, FD, MapSize,
                                                         Offset, EC));
  if (EC)
    return EC;
  return std::move(Result);
}

ErrorOr<Box<WriteThroughMemoryBuffer>>
WriteThroughMemoryBuffer::getFile(const Twine &Filename, i64 FileSize) {
  return getReadWriteFile(Filename, FileSize, FileSize, 0);
}

/// Map a subrange of the specified file as a WritableMemoryBuffer.
ErrorOr<Box<WriteThroughMemoryBuffer>>
WriteThroughMemoryBuffer::getFileSlice(const Twine &Filename, u64 MapSize,
                                       u64 Offset) {
  return getReadWriteFile(Filename, -1, MapSize, Offset);
}

template <typename MB>
static ErrorOr<Box<MB>>
getOpenFileImpl(sys::fs::file_t FD, const Twine &Filename, u64 FileSize,
                u64 MapSize, i64 Offset, bool RequiresNullTerminator,
                bool IsVolatile, Option<Align> Alignment) {
  static int PageSize = sys::Process::getPageSizeEstimate();

  // Default is to map the full file.
  if (MapSize == u64(-1)) {
    // If we don't know the file size, use fstat to find out.  fstat on an open
    // file descriptor is cheaper than stat on a random path.
    if (FileSize == u64(-1)) {
      sys::fs::file_status Status;
      std::error_code EC = sys::fs::status(FD, Status);
      if (EC)
        return EC;

      // If this not a file or a block device (e.g. it's a named pipe
      // or character device), we can't trust the size. Create the memory
      // buffer by copying off the stream.
      sys::fs::file_type Type = Status.type();
      if (Type != sys::fs::file_type::regular_file &&
          Type != sys::fs::file_type::block_file)
        return getMemoryBufferForStream(FD, Filename);

      FileSize = Status.getSize();
    }
    MapSize = FileSize;
  }

  if (shouldUseMmap(FD, FileSize, MapSize, Offset, RequiresNullTerminator,
                    PageSize, IsVolatile)) {
    std::error_code EC;
    Box<MB> Result(
        new (NamedBufferAlloc(Filename)) MemoryBufferMMapFile<MB>(
            RequiresNullTerminator, FD, MapSize, Offset, EC));
    if (!EC)
      return std::move(Result);
  }

#ifdef __MVS__
  ErrorOr<bool> NeedConversion = needzOSConversion(Filename.str().c_str(), FD);
  if (std::error_code EC = NeedConversion.getError())
    return EC;
  // File size may increase due to EBCDIC -> UTF-8 conversion, therefore we
  // cannot trust the file size and we create the memory buffer by copying
  // off the stream.
  // Note: This only works with the assumption of reading a full file (i.e,
  // Offset == 0 and MapSize == FileSize). Reading a file slice does not work.
  if (Offset == 0 && MapSize == FileSize && *NeedConversion)
    return getMemoryBufferForStream(FD, Filename);
#endif

  auto Buf =
      WritableMemoryBuffer::getNewUninitMemBuffer(MapSize, Filename, Alignment);
  if (!Buf) {
    // Failed to create a buffer. The only way it can fail is if
    // new(std::nothrow) returns 0.
    return make_error_code(errc::not_enough_memory);
  }

  // Read until EOF, zero-initialize the rest.
  MutArrayRef<char> ToRead = Buf->getBuffer();
  while (!ToRead.empty()) {
    Expected<usize> ReadBytes =
        sys::fs::readNativeFileSlice(FD, ToRead, Offset);
    if (!ReadBytes)
      return errorToErrorCode(ReadBytes.takeError());
    if (*ReadBytes == 0) {
      std::memset(ToRead.data(), 0, ToRead.size());
      break;
    }
    ToRead = ToRead.drop_front(*ReadBytes);
    Offset += *ReadBytes;
  }

  return std::move(Buf);
}

ErrorOr<Box<MemoryBuffer>>
MemoryBuffer::getOpenFile(sys::fs::file_t FD, const Twine &Filename,
                          u64 FileSize, bool RequiresNullTerminator,
                          bool IsVolatile, Option<Align> Alignment) {
  return getOpenFileImpl<MemoryBuffer>(FD, Filename, FileSize, FileSize, 0,
                                       RequiresNullTerminator, IsVolatile,
                                       Alignment);
}

ErrorOr<Box<WritableMemoryBuffer>>
WritableMemoryBuffer::getOpenFile(sys::fs::file_t FD, const Twine &Filename,
                                  u64 FileSize, bool RequiresNullTerminator,
                                  bool IsVolatile, Option<Align> Alignment) {
  return getOpenFileImpl<WritableMemoryBuffer>(FD, Filename, FileSize, FileSize,
                                               0, RequiresNullTerminator,
                                               IsVolatile, Alignment);
}

ErrorOr<Box<MemoryBuffer>> MemoryBuffer::getOpenFileSlice(
    sys::fs::file_t FD, const Twine &Filename, u64 MapSize, i64 Offset,
    bool IsVolatile, Option<Align> Alignment) {
  assert(MapSize != u64(-1));
  return getOpenFileImpl<MemoryBuffer>(FD, Filename, -1, MapSize, Offset, false,
                                       IsVolatile, Alignment);
}

ErrorOr<Box<MemoryBuffer>> MemoryBuffer::getSTDIN() {
  // Read in all of the data from stdin, we cannot mmap stdin.
  //
  // FIXME: That isn't necessarily true, we should try to mmap stdin and
  // fallback if it fails.
  sys::ChangeStdinMode(sys::fs::OF_Text);

  return getMemoryBufferForStream(sys::fs::getStdinHandle(), "<stdin>");
}

ErrorOr<Box<MemoryBuffer>>
MemoryBuffer::getFileAsStream(const Twine &Filename) {
  Expected<sys::fs::file_t> FDOrErr =
      sys::fs::openNativeFileForRead(Filename, sys::fs::OF_None);
  if (!FDOrErr)
    return errorToErrorCode(FDOrErr.takeError());
  sys::fs::file_t FD = *FDOrErr;
  ErrorOr<Box<MemoryBuffer>> Ret =
      getMemoryBufferForStream(FD, Filename);
  sys::fs::closeFile(FD);
  return Ret;
}

MemoryBufferRef MemoryBuffer::getMemBufferRef() const {
  StrRef Data = getBuffer();
  StrRef Identifier = getBufferIdentifier();
  return MemoryBufferRef(Data, Identifier);
}

SmallVecMemoryBuffer::~SmallVecMemoryBuffer() = default;
