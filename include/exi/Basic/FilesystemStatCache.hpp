//===- exi/Basic/FilesystemStatCache.hpp -----------------------------===//
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
/// Defines the FileSystemStatCache interface.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Box.hpp>
#include <core/Common/Option.hpp>
#include <core/Common/StringMap.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Support/Allocator.hpp>
#include <core/Support/Filesystem.hpp>
#include <core/Support/VirtualFilesystem.hpp>
#include <cstdint>
#include <ctime>
#include <string>
#include <utility>

namespace exi {

/// Abstract interface for introducing a FileManager cache for 'stat'
/// system calls, which is used by precompiled and pretokenized headers to
/// improve performance.
class FileSystemStatCache {
  virtual void anchor();

public:
  virtual ~FileSystemStatCache() = default;

  /// Get the 'stat' information for the specified path, using the cache
  /// to accelerate it if possible.
  ///
  /// \returns \c true if the path does not exist or \c false if it exists.
  ///
  /// If isFile is true, then this lookup should only return success for files
  /// (not directories).  If it is false this lookup should only return
  /// success for directories (not files).  On a successful file lookup, the
  /// implementation can optionally fill in \p F with a valid \p File object and
  /// the client guarantees that it will close it.
  static std::error_code get(StrRef Path, vfs::Status& Status,
                             bool isFile, Box<vfs::File>* F,
                             FileSystemStatCache* Cache,
                             vfs::FileSystem& FS, bool IsText = true);

protected:
  // FIXME: The pointer here is a non-owning/optional reference to the
  // unique_ptr. Option<unique_ptr<vfs::File>&> might be nicer, but
  // Optional needs some work to support references so this isn't possible yet.
  virtual std::error_code getStat(StrRef Path, vfs::Status& Status,
                                  bool isFile,
                                  Box<vfs::File>* F,
                                  vfs::FileSystem& FS) = 0;
};

/// A stat "cache" that can be used by FileManager to keep
/// track of the results of stat() calls that occur throughout the
/// execution of the front end.
class MemorizeStatCalls : public FileSystemStatCache {
public:
  /// The set of stat() calls that have been seen.
  StringMap<vfs::Status, BumpPtrAllocator> StatCalls;

  using iterator =
      StringMap<vfs::Status,
                      BumpPtrAllocator>::const_iterator;

  iterator begin() const { return StatCalls.begin(); }
  iterator end() const { return StatCalls.end(); }

  std::error_code getStat(StrRef Path, vfs::Status& Status,
                          bool isFile,
                          Box<vfs::File>* F,
                          vfs::FileSystem& FS) override;
};

} // namespace exi
