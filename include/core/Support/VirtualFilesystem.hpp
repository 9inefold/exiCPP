//===- Support/VirtualFilesystem.hpp ---------------------------------===//
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
/// This file defines vfs::FileSystem, a layer over the real filesystem.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Box.hpp>
#include <Common/FunctionRef.hpp>
#include <Common/IntrusiveRefCntPtr.hpp>
#include <Common/Option.hpp>
#include <Common/SmallVec.hpp>
#include <Common/String.hpp>
#include <Common/StrRef.hpp>
#include <Common/Vec.hpp>
#include <Support/Chrono.hpp>
#include <Support/Errc.hpp>
#include <Support/Error.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/ErrorOr.hpp>
#include <Support/ExtensibleRTTI.hpp>
#include <Support/Filesystem.hpp>
#include <Support/Path.hpp>
#include <ctime>
#include <memory>
#include <system_error>
#include <utility>

namespace exi {

class MemoryBuffer;
class MemoryBufferRef;
class Twine;
class WritableMemoryBuffer;

namespace vfs {

class Status {
	String Name;
	sys::fs::UniqueID UID;
	sys::TimePoint<> LastModified;
  u32 User;
  u32 Group;
  u64 Size;
  sys::fs::file_type Type = sys::fs::file_type::status_error;
  sys::fs::perms Perms;

public:
  /// Whether this entity has an external path different from the virtual path,
  /// and the external path is exposed by leaking it through the abstraction.
  /// For example, a RedirectingFileSystem will set this for paths where
  /// UseExternalName is true.
	///
  /// NOTE: Leave the path in the `Status` intact (matching the virtual path).
  bool HasExternalVFSPath = false;

	Status() = default;
	Status(const sys::fs::file_status& Stat);
	Status(const Twine& Name, sys::fs::UniqueID UID,
				 sys::TimePoint<> MTime, u32 User, u32 Group, u64 Size,
				 sys::fs::file_type Type, sys::fs::perms Perms);
	
	/// Get a copy of a Status with a different size.
  static Status CopyWithNewSize(const Status& In, u64 NewSize);
  /// Get a copy of a Status with a different name.
  static Status CopyWithNewName(const Status& In, const Twine& NewName);
  static Status CopyWithNewName(const sys::fs::file_status& In,
                                const Twine& NewName);
	
	/// Returns the name that should be used for this file or directory.
  StrRef getName() const { return Name; }

  /// @name Status interface from sys::fs
  /// @{
  sys::fs::file_type type() const { return Type; }
  sys::fs::file_type getType() const { return Type; }
  sys::fs::perms permissions() const { return Perms; }
  sys::fs::perms getPermissions() const { return Perms; }
  sys::TimePoint<> getLastModificationTime() const { return LastModified; }
  sys::fs::UniqueID getUniqueID() const { return UID; }
  u32 getUser() const { return User; }
  u32 getGroup() const { return Group; }
  u64 getSize() const { return Size; }
  /// @}
  /// @name Status queries
  /// These are static queries in sys::fs.
  /// @{
  bool equivalent(const Status& Other) const;
  bool isDirectory() const;
  bool isRegularFile() const;
  bool isOther() const;
  bool isSymlink() const;
  bool isStatusKnown() const;
  bool exists() const;
  /// @}
};

/// Represents an open file.
class File {
public:
  /// Destroy the file after closing it (if open).
  /// Sub-classes should generally call close() inside their destructors.  We
  /// cannot do that from the base class, since close is virtual.
  virtual ~File();

  /// Get the status of the file.
  virtual ErrorOr<Status> status() = 0;

  /// Get the name of the file
  virtual ErrorOr<String> getName() {
    if (auto Status = status())
      return Status->getName().str();
    else
      return Status.getError();
  }

  /// Get the contents of the file as a \p MemoryBuffer.
  virtual ErrorOr<Box<MemoryBuffer>>
   getBuffer(const Twine& Name, i64 FileSize = -1,
             bool RequiresNullTerminator = true,
						 bool IsVolatile = false,
						 bool IsMutable = false) = 0;

  /// Closes the file.
  virtual std::error_code close() = 0;

  // Get the same file with a different path.
  static ErrorOr<Box<File>>
   GetWithPath(ErrorOr<Box<File>> Result, const Twine& P);

protected:
  // Set the file's underlying path.
  virtual void setPath(const Twine& Path) {}
};

/// A member of a directory, yielded by a directory_iterator.
/// Only information available on most platforms is included.
class directory_entry {
  String Path;
  sys::fs::file_type Type = sys::fs::file_type::type_unknown;

public:
  directory_entry() = default;
  directory_entry(String Path, sys::fs::file_type Type) :
	 Path(std::move(Path)), Type(Type) {}

  StrRef path() const { return Path; }
  sys::fs::file_type type() const { return Type; }
};

namespace H {

/// An interface for virtual file systems to provide an iterator over the
/// (non-recursive) contents of a directory.
struct DirIterImpl {
  virtual ~DirIterImpl();

  /// Sets \c CurrentEntry to the next entry in the directory on success,
  /// to directory_entry() at end,  or returns a system-defined \c error_code.
  virtual std::error_code increment() = 0;

  directory_entry CurrentEntry;
};

} // namespace H

/// An input iterator over the entries in a virtual path, similar to
/// sys::fs::directory_iterator.
class directory_iterator {
	/// Input iterator semantics on copy
  std::shared_ptr<H::DirIterImpl> Impl;

public:
  directory_iterator(std::shared_ptr<H::DirIterImpl> I) : Impl(std::move(I)) {
    exi_assert(Impl.get() != nullptr,
							"requires non-null implementation");
    if (Impl->CurrentEntry.path().empty())
      Impl.reset(); // Normalize the end iterator to Impl == nullptr.
  }

  /// Construct an 'end' iterator.
  directory_iterator() = default;

  /// Equivalent to operator++, with an error code.
  directory_iterator& increment(std::error_code& EC) {
    exi_assert(Impl, "attempting to increment past end");
    EC = Impl->increment();
    if (Impl->CurrentEntry.path().empty())
      Impl.reset(); // Normalize the end iterator to Impl == nullptr.
    return *this;
  }

  const directory_entry& operator*() const { return Impl->CurrentEntry; }
  const directory_entry* operator->() const { return &Impl->CurrentEntry; }

  bool operator==(const directory_iterator& RHS) const {
    if (Impl && RHS.Impl)
      return Impl->CurrentEntry.path() == RHS.Impl->CurrentEntry.path();
    return !Impl && !RHS.Impl;
  }
  bool operator!=(const directory_iterator& RHS) const {
    return !(*this == RHS);
  }
};

class FileSystem;

namespace H {

/// Keeps state for the recursive_directory_iterator.
struct RecDirIterState {
  Vec<directory_iterator> Stack;
  bool HasNoPushRequest = false;
};

} // end namespace H

/// An input iterator over the recursive contents of a virtual path,
/// similar to sys::fs::recursive_directory_iterator.
class recursive_directory_iterator {
  FileSystem* FS;
	/// Input iterator semantics on copy.
  std::shared_ptr<H::RecDirIterState> State;

public:
  recursive_directory_iterator(FileSystem& InFS, const Twine& Path,
                               std::error_code& EC);

  /// Construct an 'end' iterator.
  recursive_directory_iterator() = default;

  /// Equivalent to operator++, with an error code.
  recursive_directory_iterator &increment(std::error_code& EC);

  const directory_entry& operator*() const { return *State->Stack.back(); }
  const directory_entry* operator->() const { return &*State->Stack.back(); }

  bool operator==(const recursive_directory_iterator& Other) const {
    return this->State == Other.State; // identity
  }
  bool operator!=(const recursive_directory_iterator& RHS) const {
    return !(*this == RHS);
  }

  /// Gets the current level. Starting path is at level 0.
  int level() const {
    exi_assert(!State->Stack.empty(),
           		"Cannot get level without any iteration state");
    return State->Stack.size() - 1;
  }

  void no_push() { State->HasNoPushRequest = true; }
};

/// The virtual file system interface.
class FileSystem : public ThreadSafeRefCountedBase<FileSystem>,
                   public RTTIExtends<FileSystem, RTTIRoot> {
public:
  static const char ID;
  virtual ~FileSystem();

  /// Get the status of the entry at \p Path, if one exists.
  virtual ErrorOr<Status> status(const Twine& Path) = 0;

  /// Get a \p File object for the text file at \p Path, if one exists.
  virtual ErrorOr<Box<File>> openFileForRead(const Twine& Path) = 0;

  /// Get a \p File object for the binary file at \p Path, if one exists.
  /// Some non-ascii based file systems perform encoding conversions
  /// when reading as a text file, and this function should be used if
  /// a file's bytes should be read as-is. On most filesystems, this
  /// is the same behaviour as openFileForRead.
  virtual ErrorOr<Box<File>> openFileForReadBinary(const Twine& Path) {
    return openFileForRead(Path);
  }

  /// This is a convenience method that opens a file, gets its content and then
  /// closes the file.
  /// The IsText parameter is used to distinguish whether the file should be
  /// opened as a binary or text file.
  ErrorOr<Box<MemoryBuffer>>
   getBufferForFile(const Twine& Name, i64 FileSize = -1,
                    bool RequiresNullTerminator = true, bool IsVolatile = false,
                    bool IsText = true, bool IsMutable = false);

  /// Get a directory_iterator for \p Dir.
  /// \note The 'end' iterator is directory_iterator().
  virtual directory_iterator dir_begin(const Twine& Dir,
                                       std::error_code& EC) = 0;

  /// Set the working directory. This will affect all following operations on
  /// this file system and may propagate down for nested file systems.
  virtual std::error_code setCurrentWorkingDirectory(const Twine& Path) = 0;

  /// Get the working directory of this file system.
  virtual ErrorOr<std::string> getCurrentWorkingDirectory() const = 0;

  /// Gets real path of \p Path e.g. collapse all . and .. patterns, resolve
  /// symlinks. For real file system, this uses `sys::fs::real_path`.
  /// This returns errc::operation_not_permitted if not implemented by subclass.
  virtual std::error_code getRealPath(const Twine& Path,
                                      SmallVecImpl<char>& Output);

  /// Check whether \p Path exists. By default this uses \c status(), but
  /// filesystems may provide a more efficient implementation if available.
  virtual bool exists(const Twine& Path);

  /// Is the file mounted on a local filesystem?
  virtual std::error_code isLocal(const Twine& Path, bool& Result);

  /// Make \a Path an absolute path.
  ///
  /// Makes \a Path absolute using the current directory if it is not already.
  /// An empty \a Path will result in the current directory.
  ///
  /// /absolute/path   => /absolute/path
  /// relative/../path => <current-directory>/relative/../path
  ///
  /// \param Path A path that is modified to be an absolute path.
  /// \returns success if \a path has been made absolute, otherwise a
  ///          platform-specific error_code.
  virtual std::error_code makeAbsolute(SmallVecImpl<char>& Path) const;

  /// \returns true if \p A and \p B represent the same file, or an error or
  /// false if they do not.
  ErrorOr<bool> equivalent(const Twine& A, const Twine& B);

  enum class PrintType { Summary, Contents, RecursiveContents };
  void print(raw_ostream& OS, PrintType Type = PrintType::Contents,
             unsigned IndentLevel = 0) const {
    printImpl(OS, Type, IndentLevel);
  }

  using VisitCallbackTy = function_ref<void(FileSystem&)>;
  virtual void visitChildFileSystems(VisitCallbackTy Callback) {}
  void visit(VisitCallbackTy Callback) {
    Callback(*this);
    visitChildFileSystems(Callback);
  }

#if !defined(NDEBUG) || defined(EXI_ENABLE_DUMP)
  EXI_DUMP_METHOD void dump() const;
#endif

protected:
  virtual void printImpl(raw_ostream& OS, PrintType Type,
                         unsigned IndentLevel) const {
    printIndent(OS, IndentLevel);
    OS << "FileSystem\n";
  }

  void printIndent(raw_ostream& OS, unsigned IndentLevel) const {
    for (unsigned i = 0; i < IndentLevel; ++i)
      OS << "  ";
  }
};

/// Gets an \p vfs::FileSystem for the 'real' file system, as seen by
/// the operating system.
/// The working directory is linked to the process's working directory.
/// (This is usually thread-hostile).
IntrusiveRefCntPtr<FileSystem> getRealFileSystem();

/// Create an \p vfs::FileSystem for the 'real' file system, as seen by
/// the operating system.
/// It has its own working directory, independent of (but initially equal to)
/// that of the process.
Box<FileSystem> createPhysicalFileSystem();

// TODO: Add extensions to vfs (OverlayFileSystem)

} // namespace vfs
} // namespace exi;
