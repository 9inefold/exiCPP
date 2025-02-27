//===- exi/Basic/FileManager.cpp -------------------------------------===//
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
/// This file defines an interface for dealing with files.
///
//===----------------------------------------------------------------===//

#include <exi/Basic/FileManager.hpp>
#include <core/Common/Box.hpp>
#include <core/Common/Option.hpp>
#include <core/Common/STLExtras.hpp>
#include <core/Common/SmallStr.hpp>
#include <core/Common/String.hpp>
#include <core/Support/Debug.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/Filesystem.hpp>
#include <core/Support/IntCast.hpp>
#include <core/Support/Logging.hpp>
#include <core/Support/MemoryBuffer.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/FilesystemStatCache.hpp>

#define DEBUG_TYPE "FileManager"

using namespace exi;

namespace {
constexpr int kStartingCacheSize = 4;
constexpr int kMaxVirtualDirDepth = 126; // May change...
} // namespace `anonymous`

FileManager::FileManager(const FileSystemOptions& Opts,
                         IntrusiveRefCntPtr<vfs::FileSystem> VFS) :
 FS(std::move(VFS)), FileSystemOpts(Opts),
 SeenDirEntries(kStartingCacheSize),
 SeenFileEntries(kStartingCacheSize)
{
  if (!this->FS)
    // Get the default real filesystem.
    this->FS = vfs::getRealFileSystem();
}

FileManager::~FileManager() = default;

/// Retrieve the directory that the given file name resides in.
/// Filename can point to either a real file or a virtual file.
static Expected<DirectoryEntryRef>
getDirectoryFromFile(FileManager& FileMgr, StrRef Filename,
                     bool CacheFailures) {
  if (Filename.empty())
    return errorCodeToError(
        make_error_code(std::errc::no_such_file_or_directory));

  if (sys::path::is_separator(Filename[Filename.size() - 1]))
    return errorCodeToError(make_error_code(std::errc::is_a_directory));

  StrRef DirName = sys::path::parent_path(Filename);
  // Use the current directory if file has no path component.
  if (DirName.empty())
    DirName = ".";

  return FileMgr.getDirectoryRef(DirName, CacheFailures);
}

DirectoryEntry*& FileManager::getRealDirEntry(const vfs::Status& Status) {
  exi_assert(Status.isDirectory(), "The directory should exist!");
  // See if we have already opened a directory with the
  // same inode (this occurs on Unix-like systems when one dir is
  // symlinked to another, for example) or the same path (on
  // Windows).
  DirectoryEntry*& UDE = UniqueRealDirs[Status.getUniqueID()];

  if (!UDE) {
    // We don't have this directory yet, add it.  We use the string
    // key from the SeenDirEntries map as the string.
    UDE = new (DirsAlloc.Allocate()) DirectoryEntry();
  }
  return UDE;
}

bool FileManager::addAsVirtualDir(StrRef DirName) {
  if (DirName.empty())
    DirName = ".";
  
  auto& NamedDirEnt = *SeenDirEntries.insert(
        {DirName, std::errc::no_such_file_or_directory}).first;

  /// Already in cache, exit early.
  if (NamedDirEnt.second)
    return true;

  // Check to see if the directory exists.
  vfs::Status Status;
  auto statError =
      getStatValue(DirName, Status, false, std::nullopt /*directory lookup*/);
  if (statError) {
    // There's no real directory at the given path.
    // Add the virtual directory to the cache.
    auto* UDE = new (DirsAlloc.Allocate()) DirectoryEntry();
    NamedDirEnt.second = *UDE;
    VirtualDirectoryEntries.push_back(UDE);
  } else {
    // There is the real directory
    DirectoryEntry*& UDE = getRealDirEntry(Status);
    NamedDirEnt.second = *UDE;
  }

  return false;
}

/// Add all ancestors of the given path (pointing to either a file or
/// a directory) as virtual directories.
void FileManager::addAncestorsAsVirtualDirs(StrRef Path) {
  int DirDepth = 0;
  while (true) {
    StrRef DirName = sys::path::parent_path(Path);
    // When caching a virtual directory, we always cache its ancestors
    // at the same time.  Therefore, if DirName is already in the cache,
    // we don't need to recurse as its ancestors must also already be in
    // the cache (or it's a known non-virtual directory).
    if (addAsVirtualDir(DirName))
      return;

    // This is unlikely because most file structures will
    // never reach this depth. If they do, there's probably something wrong.
    // The bailout depth is 126 by default, but this can be changed.
    if EXI_UNLIKELY(++DirDepth > kMaxVirtualDirDepth)
      break;
    Path = DirName;
  }

  // This will rarely be reached.
  LOG_WARN("Virtual path bailout depth of {} reached: {}",
           kMaxVirtualDirDepth, Path);
}

Expected<DirectoryEntryRef>
 FileManager::getDirectoryRef(StrRef DirName, bool CacheFailures) {
  // stat doesn't like trailing separators except for root directory.
  // At least, on Win32 MSVCRT, stat() cannot strip trailing '/'.
  // (though it can strip '\\')
  if (DirName.size() > 1 &&
      DirName != sys::path::root_path(DirName) &&
      sys::path::is_separator(DirName.back()))
    DirName = DirName.substr(0, DirName.size()-1);
  Option<String> DirNameStr;
  if (is_style_windows(sys::path::Style::native)) {
    // Fixing a problem with "exe C:test.c" on Windows.
    // Stat("C:") does not recognize "C:" as a valid directory
    if (DirName.size() > 1 && DirName.back() == ':' &&
        DirName.equals_insensitive(sys::path::root_name(DirName))) {
      DirNameStr = DirName.str() + '.';
      DirName = *DirNameStr;
    }
  }

  ++NDirLookups;

  // See if there was already an entry in the map.  Note that the map
  // contains both virtual and real directories.
  auto SeenDirInsertResult =
      SeenDirEntries.insert({DirName, std::errc::no_such_file_or_directory});
  if (!SeenDirInsertResult.second) {
    if (SeenDirInsertResult.first->second)
      return DirectoryEntryRef(*SeenDirInsertResult.first);
    return errorCodeToError(SeenDirInsertResult.first->second.getError());
  }

  // We've not seen this before. Fill it in.
  ++NDirCacheMisses;
  auto& NamedDirEnt = *SeenDirInsertResult.first;
  exi_assert(!NamedDirEnt.second, "should be newly-created");

  // Get the null-terminated directory name as stored as the key of the
  // SeenDirEntries map.
  StrRef InternedDirName = NamedDirEnt.first();

  // Check to see if the directory exists.
  vfs::Status Status;
  auto statError = getStatValue(InternedDirName, Status, false,
                                std::nullopt /*directory lookup*/);
  if (statError) {
    // There's no real directory at the given path.
    if (CacheFailures)
      NamedDirEnt.second = statError;
    else
      SeenDirEntries.erase(DirName);
    return errorCodeToError(statError);
  }

  // It exists.
  DirectoryEntry*& UDE = getRealDirEntry(Status);
  NamedDirEnt.second = *UDE;

  return DirectoryEntryRef(NamedDirEnt);
}

Expected<FileEntryRef> FileManager::getFileRefEx(StrRef Filename,
                                                 bool OpenFile,
                                                   bool CacheFailures,
                                                 bool IsText,
                                                 bool RemapExtern) {
  ++NFileLookups;

  // See if there is already an entry in the map.
  auto [SeenFileEntry, DidInsert] =
      SeenFileEntries.insert({Filename, std::errc::no_such_file_or_directory});
  if (!DidInsert) {
    if (!SeenFileEntry->second)
      return errorCodeToError(
          SeenFileEntry->second.getError());
    return FileEntryRef(*SeenFileEntry);
  }

  // We've not seen this before. Fill it in.
  ++NFileCacheMisses;
  FileEntryRef::MapEntry* NamedFileEnt = &*SeenFileEntry;
  exi_assert(!NamedFileEnt->second, "should be newly-created");

  // Get the null-terminated file name as stored as the key of the
  // SeenFileEntries map.
  StrRef InternedFileName = NamedFileEnt->first();

  // Look up the directory for the file.  When looking up something like
  // sys/foo.h we'll discover all of the search directories that have a 'sys'
  // subdirectory.  This will let us avoid having to waste time on known-to-fail
  // searches when we go to find sys/bar.h, because all the search directories
  // without a 'sys' subdir will get a cached failure result.
  auto DirInfoOrErr = getDirectoryFromFile(*this, Filename, CacheFailures);
  if (!DirInfoOrErr) { // Directory doesn't exist, file can't exist.
    std::error_code Err = errorToErrorCode(DirInfoOrErr.takeError());
    if (CacheFailures)
      NamedFileEnt->second = Err;
    else
      SeenFileEntries.erase(Filename);

    return errorCodeToError(Err);
  }
  DirectoryEntryRef DirInfo = *DirInfoOrErr;

  // FIXME: Use the directory info to prune this, before doing the stat syscall.
  // FIXME: This will reduce the # syscalls.

  // Check to see if the file exists.
  std::unique_ptr<vfs::File> F;
  vfs::Status Status;
  auto statError = getStatValue(InternedFileName, Status, true,
                                OpenFile ? &F : nullptr, IsText);
  if (statError) {
    // There's no real file at the given path.
    if (CacheFailures)
      NamedFileEnt->second = statError;
    else
      SeenFileEntries.erase(Filename);

    return errorCodeToError(statError);
  }

  exi_assert(OpenFile || !F, "undesired open file");

  // It exists. See if we have already opened a file with the same inode.
  // This occurs when one dir is symlinked to another, for example.
  FileEntry*& UFE = UniqueRealFiles[Status.getUniqueID()];
  bool ReusingEntry = UFE != nullptr;
  if (!UFE)
    UFE = new (FilesAlloc.Allocate()) FileEntry();

  const bool ShouldRemap = (RemapExtern && Status.HasExternalVFSPath);
  if (!ShouldRemap || Status.getName() == Filename) {
    // Use the requested name. Set the FileEntry.
    NamedFileEnt->second = FileEntryRef::MapValue(*UFE, DirInfo);
  } else {
    // Name mismatch. We need a redirect. First grab the actual entry we want
    // to return.
    //
    // This redirection logic intentionally leaks the external name of a
    // redirected file that uses 'use-external-name' in
    // vfs::RedirectionFileSystem. This allows clang to report the external
    // name to users (in diagnostics) and to tools that don't have access to
    // the VFS (in debug info and dependency '.d' files).
    //
    // FIXME: This is pretty complex and has some very complicated interactions.
    // It's also inconsistent with how "real" filesystems behave and confuses
    // parts that expect to see the name-as-accessed on the FileEntryRef.
    //
    // A potential plan to remove this is as follows -
    //   - Update the VFS to always return the requested name. This could also
    //     return the external name, or just have an API to request it
    //     lazily. The latter has the benefit of making accesses of the
    //     external path easily tracked, but may also require extra work than
    //     just returning up front.
    //   - (Optionally) Add an API to VFS to get the external filename lazily
    //     and update `FileManager::getExternalPath()` to use it instead. This
    //     has the benefit of making such accesses easily tracked, though isn't
    //     necessarily required (and could cause extra work than just adding to
    //     eg. `vfs::Status` up front).
    auto& Redirection =
        *SeenFileEntries
             .insert({Status.getName(), FileEntryRef::MapValue(*UFE, DirInfo)})
             .first;
    exi_assert(isa<FileEntry*>(Redirection.second->V),
               "filename redirected to a non-canonical filename?");
    exi_assert(cast<FileEntry*>(Redirection.second->V) == UFE,
               "filename from getStatValue() refers to wrong file");

    // Cache the redirection in the previously-inserted entry, still available
    // in the tentative return value.
    NamedFileEnt->second = FileEntryRef::MapValue(Redirection, DirInfo);
  }

  FileEntryRef ReturnedRef(*NamedFileEnt);
  if (ReusingEntry) { // Already have an entry with this inode, return it.
    return ReturnedRef;
  }

  // Otherwise, we don't have this file yet, add it.
  UFE->Size = Status.getSize();
  UFE->Dir = &DirInfo.getDirEntry();
  UFE->UniqueID = Status.getUniqueID();
  UFE->IsNamedPipe = Status.getType() == sys::fs::file_type::fifo_file;
  UFE->TheFile = std::move(F);

  UFE->IsMutable = false;
  UFE->IsVolatile = false;
  UFE->BufferOverridden = false;
  UFE->IsDirty = false;

  if (UFE->TheFile) {
    if (auto PathName = UFE->TheFile->getName())
      fillAbsolutePathName(UFE, *PathName);
  } else if (!OpenFile) {
    // We should still fill the path even if we aren't opening the file.
    fillAbsolutePathName(UFE, InternedFileName);
  }
  return ReturnedRef;
}

Expected<FileEntryRef> FileManager::getFileRef(StrRef Filename,
                                               bool OpenFile,
                                               bool CacheFailures,
                                               bool IsText) {
  return this->getFileRefEx(Filename, OpenFile,
                            CacheFailures, IsText, /*RemapExtern=*/false);
}

Expected<FileEntryRef> FileManager::getExternalFileRef(StrRef Filename,
                                                         bool OpenFile,
                                                         bool CacheFailures,
                                                         bool IsText) {
  return this->getFileRefEx(Filename, OpenFile,
                            CacheFailures, IsText, /*RemapExtern=*/true);
}

bool FileManager::fixupRelativePath(SmallVecImpl<char>& Path) const {
  StrRef PathRef(Path.data(), Path.size());
  if (!FileSystemOpts.WorkingDir
      || sys::path::is_absolute(PathRef))
    return false;

  SmallStr<128> NewPath(*FileSystemOpts.WorkingDir);
  sys::path::append(NewPath, PathRef);
  Path = NewPath;
  return true;
}

bool FileManager::makeAbsolutePath(SmallVecImpl<char>& Path) const {
  bool Changed = fixupRelativePath(Path);

  if (!sys::path::is_absolute(StrRef(Path.data(), Path.size()))) {
    FS->makeAbsolute(Path);
    Changed = true;
  }

  return Changed;
}

void FileManager::fillAbsolutePathName(FileEntry* UFE, StrRef FileName) {
  SmallStr<128> AbsPath(FileName);
  // This is not the same as `VFS::getRealPath()`, which resolves symlinks
  // but can be very expensive on real file systems.
  makeAbsolutePath(AbsPath);
  sys::path::remove_dots(AbsPath, /*remove_dot_dot=*/true);
  UFE->ExternalName = std::string(AbsPath);
}

ErrorOr<Box<MemoryBuffer>>
 FileManager::getBufferForFile(FileEntryRef FE, bool isVolatile,
                               bool RequiresNullTerminator,
                               Option<i64> MaybeLimit, bool IsText) {
  const FileEntry* Entry = &FE.getFileEntry();
  // If the content is living on the file entry, return a reference to it.
  if (auto& MB = Entry->TheBuffer)
    return MemoryBuffer::getMemBuffer(MB->getMemBufferRef());

  u64 FileSize = Entry->getSize();

  if (MaybeLimit)
    /// Ensure the cast can be done losslessly.
    FileSize = IntCast<u64>(*MaybeLimit);

  // If there's a high enough chance that the file have changed since we
  // got its size, force a stat before opening it.
  if (isVolatile || Entry->isNamedPipe())
    FileSize = -1;

  StrRef Filename = FE.getName();
  // If the file is already open, use the open file descriptor.
  if (auto& F = Entry->TheFile) {
    auto Result = F->getBuffer(Filename, FileSize,
                               RequiresNullTerminator, isVolatile);
    Entry->closeFile();
    return Result;
  }

  // Otherwise, open the file.
  return getBufferForFileImpl(Filename, FileSize, isVolatile,
                              RequiresNullTerminator, IsText);
}

Error FileManager::loadBufferForFileImpl(FileEntryRef FE, bool isVolatile,
                                          bool RequiresNullTerminator,
                                          Option<i64> MaybeLimit,
                                         bool IsText, bool IsMutable) {
  const FileEntry* Entry = &FE.getFileEntry();
  bool HadBuffer = false;
  
  // If the content is living on the file entry, return a reference to it.
  if (Entry->TheBuffer) {
    // If the file doesn't need to be mutable, or if it's already mutable,
    // we can return early. Otherwise we have to reload the buffer.
    if (!IsMutable || Entry->IsMutable)
      return Error::success();
    HadBuffer = true;
  }

  FileEntry* MFE = &getOptionalRef(Entry)
    .expect("Invalid entry in loadBufferForImpl.");

  u64 FileSize = Entry->getSize();

  if (MaybeLimit)
    /// Ensure the cast can be done losslessly.
    FileSize = IntCast<u64>(*MaybeLimit);

  // If there's a high enough chance that the file have changed since we
  // got its size, force a stat before opening it.
  if (isVolatile || Entry->isNamedPipe())
    FileSize = -1;

  StrRef Filename = FE.getName();
  // If the file is already open, use the open file descriptor.
  if (Entry->TheFile /*&& !IsMutable*/) {
    auto& F = Entry->TheFile;
    auto Result = F->getBuffer(Filename, FileSize,
                               RequiresNullTerminator, isVolatile, IsMutable);
    Entry->closeFile();
    if (auto EC = Result.getError())
      return errorCodeToError(EC);
    Entry->TheBuffer = std::move(*Result);
  } else {
    auto Result = getBufferForFileImpl(Filename, FileSize, isVolatile,
                                       RequiresNullTerminator, IsText,
                                       IsMutable);
    if (auto EC = Result.getError())
      return errorCodeToError(EC);
    Entry->TheBuffer = std::move(*Result);
  }

  MFE->IsMutable = IsMutable;
  MFE->IsVolatile = isVolatile;
  MFE->BufferOverridden = HadBuffer;
  MFE->IsDirty = false;

  return Error::success();
}

ErrorOr<Box<MemoryBuffer>>
 FileManager::getBufferForFileImpl(StrRef Filename, i64 FileSize,
                                   bool isVolatile, bool RequiresNullTerminator,
                                   bool IsText, bool IsMutable) const {
  if (!FileSystemOpts.WorkingDir)
    return FS->getBufferForFile(Filename, FileSize, RequiresNullTerminator,
                                isVolatile, IsText, IsMutable);

  SmallStr<128> FilePath(Filename);
  fixupRelativePath(FilePath);
  return FS->getBufferForFile(FilePath, FileSize, RequiresNullTerminator,
                              isVolatile, IsText, IsMutable);
}

Option<FileEntry&> FileManager::getOptionalRef(const FileEntry* Entry) const {
  // Assume things passed in are from this FileManager.
  if EXI_UNLIKELY(!FilesAlloc.identifyObject(Entry)) {
    LOG_WARN("The entry '{}' is not located in this FileManager!",
      Entry->getFilename());
    return std::nullopt;
  }
  return *const_cast<FileEntry*>(Entry);
}

/// getStatValue - Get the 'stat' information for the specified path,
/// using the cache to accelerate it if possible.  This returns true
/// if the path points to a virtual file or does not exist, or returns
/// false if it's an existent real file.  If FileDescriptor is NULL,
/// do directory look-up instead of file look-up.
std::error_code FileManager::getStatValue(StrRef Path,
                                          vfs::Status& Status,
                                          bool isFile,
                                          Option<Box<vfs::File>&> F,
                                          bool IsText) {
  SmallStr<128> FilePath(Path);
  fixupRelativePath(FilePath);

  return FileSystemStatCache::Get(FilePath.n_str(), Status, isFile, F,
                                  StatCache.get(), *FS, IsText);
}

void FileManager::PrintStats() const {
  errs() << "\n*** File Manager Stats:\n";
  errs() << UniqueRealFiles.size() << " real files found, "
         << UniqueRealDirs.size() << " real dirs found.\n";
  errs() << VirtualFileEntries.size() << " virtual files found, "
         << VirtualDirectoryEntries.size() << " virtual dirs found.\n";
  errs() << NDirLookups << " dir lookups, "
         << NDirCacheMisses << " dir cache misses.\n";
  errs() << NFileLookups << " file lookups, "
         << NFileCacheMisses << " file cache misses.\n";

  getVirtualFileSystem().visit([](vfs::FileSystem& VFS) {
    /* TODO: TracingFileSystem?
    if (auto* T = dyn_cast_or_null<vfs::TracingFileSystem>(&VFS))
      errs() << "\n*** Virtual File System Stats:\n"
             << T->NumStatusCalls << " status() calls\n"
             << T->NumOpenFileForReadCalls << " openFileForRead() calls\n"
             << T->NumDirBeginCalls << " dir_begin() calls\n"
             << T->NumGetRealPathCalls << " getRealPath() calls\n"
             << T->NumExistsCalls << " exists() calls\n"
             << T->NumIsLocalCalls << " isLocal() calls\n";
    */
  });
}
