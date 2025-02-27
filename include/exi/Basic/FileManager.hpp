//===- exi/Basic/FileManager.hpp ------------------------------------===//
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
/// This file defines an interface for dealing with files.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Box.hpp>
#include <core/Common/DenseMap.hpp>
#include <core/Common/Fundamental.hpp>
#include <core/Common/IntrusiveRefCntPtr.hpp>
#include <core/Common/SmallVec.hpp>
#include <core/Common/StringMap.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Support/Allocator.hpp>
#include <core/Support/ErrorOr.hpp>
#include <core/Support/Filesystem.hpp>
#include <core/Support/VirtualFilesystem.hpp>
#include <exi/Basic/FileEntry.hpp>
#include <exi/Basic/FilesystemStatCache.hpp>

namespace exi {

class XMLManager;

struct FileSystemOptions {
  Option<String> WorkingDir;
};

/// Implements support for file system lookup, file system caching,
/// and directory search management.
class FileManager : public RefCountedBase<FileManager> {
  friend class XMLManager;
  IntrusiveRefCntPtr<vfs::FileSystem> FS;
  FileSystemOptions FileSystemOpts;
  SpecificBumpPtrAllocator<FileEntry> FilesAlloc;
  SpecificBumpPtrAllocator<DirectoryEntry> DirsAlloc;

  /// Cache for existing real directories.
  DenseMap<sys::fs::UniqueID, DirectoryEntry*> UniqueRealDirs;

  /// Cache for existing real files.
  DenseMap<sys::fs::UniqueID, FileEntry*> UniqueRealFiles;

  /// The virtual directories that we have allocated.
  ///
  /// For each virtual file (e.g. foo/bar/baz.cpp), we add all of its parent
  /// directories (foo/ and foo/bar/) here.
  SmallVec<DirectoryEntry*, 4> VirtualDirectoryEntries;
  /// The virtual files that we have allocated.
  SmallVec<FileEntry*, 4> VirtualFileEntries;

  /// A cache that maps paths to directory entries (either real or virtual)
  /// we have looked up, or an error that occurred when we looked up
  /// the directory.
  ///
  /// The actual Entries for real directories/files are
  /// owned by `UniqueRealDirs`/`UniqueRealFiles` above, while the Entries
  /// for virtual directories/files are owned by
  /// `VirtualDirectoryEntries`/`VirtualFileEntries` above.
  StringMap<DirectoryEntryRef::MapStore, BumpPtrAllocator> SeenDirEntries;

  /// A cache that maps paths to file entries (either real or virtual) 
  /// we have looked up, or an error that occurred when we looked up the file.
  ///
  /// @see SeenDirEntries
  StringMap<ErrorOr<FileEntryRef::MapValue>, BumpPtrAllocator> SeenFileEntries;

  /// Statistics gathered during the lifetime of the FileManager.
  unsigned NDirLookups = 0;
  unsigned NFileLookups = 0;
  unsigned NDirCacheMisses = 0;
  unsigned NFileCacheMisses = 0;

  // Caching.
  Box<FileSystemStatCache> StatCache;

  Expected<FileEntryRef>
   getFileRefEx(StrRef Filename, bool OpenFile,
                 bool CacheFailures, bool IsText, bool RemapExtern);

  std::error_code getStatValue(StrRef Path, vfs::Status& Status,
                               bool isFile, Option<Box<vfs::File>&> F,
                               bool IsText = true);
  
  /// Add single path as a virtual directory. Returns `true` if in cache.
  bool addAsVirtualDir(StrRef DirName);

  /// Add all ancestors of the given path (pointing to either a file
  /// or a directory) as virtual directories.
  void addAncestorsAsVirtualDirs(StrRef Path);

  /// Fills the Filename in file entry.
  void fillAbsolutePathName(FileEntry* UFE, StrRef FileName);
  
public:
  /// Create a FileManager with an optional VFS.
  /// If no VFS is provided, the `RealFileSystem` will be used.
  FileManager(const FileSystemOptions& Opts,
              IntrusiveRefCntPtr<vfs::FileSystem> VFS = nullptr);
  ~FileManager();

  /// Returns the number of unique real file entries cached by the file manager.
  usize getNumUniqueRealFiles() const { return UniqueRealFiles.size(); }

  /// Lookup, cache, and verify the specified directory (real or virtual).
  ///
  /// This returns a `std::error_code` if there was an error reading the
  /// directory. On success, returns the reference to the directory entry
  /// together with the exact path that was used to access a file by a
  /// particular call to getDirectoryRef.
  ///
  /// @param CacheFailures If true and the file does not exist, we'll cache
  /// the failure to find this file.
  Expected<DirectoryEntryRef> getDirectoryRef(StrRef DirName,
                                              bool CacheFailures = true);

  /// Get a `DirectoryEntryRef` if it exists, without doing anything on error.
  OptionalDirectoryEntryRef getOptionalDirectoryRef(StrRef DirName,
                                                    bool CacheFailures = true) {
    return expectedToOptional(getDirectoryRef(DirName, CacheFailures));
  }

  /// Lookup, cache, and verify the specified file (real or virtual). Return the
  /// reference to the file entry together with the exact path that was used to
  /// access a file by a particular call to getFileRef.
  ///
  /// This returns a `std::error_code` if there was an error loading the file,
  /// or a `FileEntryRef` otherwise.
  ///
  /// @param OpenFile if true and the file exists, it will be opened.
  ///
  /// @param CacheFailures If true and the file does not exist, we'll cache
  /// the failure to find this file.
  Expected<FileEntryRef> getFileRef(StrRef Filename,
                                    bool OpenFile = false,
                                    bool CacheFailures = true,
                                    bool IsText = true);

  /// Get a `FileEntryRef` if it exists, without doing anything on error.
  OptionalFileEntryRef getOptionalFileRef(StrRef Filename,
                                          bool OpenFile = false,
                                          bool CacheFailures = true) {
    return expectedToOptional(getFileRef(Filename, OpenFile, CacheFailures));
  }

  /// Lookup, cache, and verify the specified file (real or virtual). Return the
  /// reference to the file entry together with the exact path that was used to
  /// access a file by a particular call to getFileRef. If the underlying VFS is
  /// a redirecting VFS that uses external file names, the returned FileEntryRef
  /// will use the external name instead of the filename that was passed to this
  /// method.
  ///
  /// This returns a `std::error_code` if there was an error loading the file,
  /// or a `FileEntryRef` otherwise.
  ///
  /// @param OpenFile if true and the file exists, it will be opened.
  ///
  /// @param CacheFailures If true and the file does not exist, we'll cache
  /// the failure to find this file.
  Expected<FileEntryRef> getExternalFileRef(StrRef Filename,
                                            bool OpenFile = false,
                                            bool CacheFailures = true,
                                            bool IsText = true);

  /// Get a `FileEntryRef` if it exists, without doing anything on error.
  OptionalFileEntryRef getOptionalExternalFileRef(StrRef Filename,
                                                  bool OpenFile = false,
                                                  bool CacheFailures = true) {
    return expectedToOptional(
      getExternalFileRef(Filename, OpenFile, CacheFailures));
  }

  vfs::FileSystem& getVirtualFileSystem() const { return *FS; }
  IntrusiveRefCntPtr<vfs::FileSystem>
   getVirtualFileSystemPtr() const { return FS; }

  void setVirtualFileSystem(IntrusiveRefCntPtr<vfs::FileSystem> VFS) {
    this->FS = std::move(VFS);
  }

  ErrorOr<Box<MemoryBuffer>>
   getBufferForFile(FileEntryRef FE, bool isVolatile = false,
                     bool RequiresNullTerminator = true,
                    Option<i64> MaybeLimit = std::nullopt,
                    bool IsText = true);
  
  template <typename MB = MemoryBuffer>
  Error loadBufferForFile(FileEntryRef FE, bool isVolatile = false,
                           bool RequiresNullTerminator = true,
                           Option<i64> MaybeLimit = std::nullopt,
                           bool IsText = true) {
    return loadBufferForFileImpl(FE, isVolatile,
      RequiresNullTerminator, MaybeLimit, IsText,
      kMemoryBufferMutability<MB>);
  }

private:
  ErrorOr<Box<MemoryBuffer>>
   getBufferForFileImpl(StrRef Filename, i64 FileSize, bool isVolatile,
                        bool RequiresNullTerminator,
                        bool IsText, bool IsMutable = false) const;
  
  Error loadBufferForFileImpl(FileEntryRef FE, bool isVolatile = false,
                                bool RequiresNullTerminator = true,
                               Option<i64> MaybeLimit = std::nullopt,
                               bool IsText = true, bool IsMutable = false);

  Option<FileEntry&> getOptionalRef(const FileEntry* Entry) const;
  DirectoryEntry*& getRealDirEntry(const vfs::Status& Status);

public:

  /// If path is not absolute and FileSystemOptions set the working
  /// directory, the path is modified to be relative to the given
  /// working directory.
  /// @return true if `Path` changed.
  bool fixupRelativePath(SmallVecImpl<char>& Path) const;

  /// Makes `Path` absolute taking into account FileSystemOptions and the
  /// working directory option.
  /// @return true if `Path` changed to absolute.
  bool makeAbsolutePath(SmallVecImpl<char>& Path) const;

  void PrintStats() const;
};

} // namespace exi
