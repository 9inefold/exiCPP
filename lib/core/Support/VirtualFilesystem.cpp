//===- Support/VirtualFilesystem.cpp ---------------------------------===//
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

#include <Support/VirtualFilesystem.hpp>
#include <Common/ArrayRef.hpp>
#include <Common/Box.hpp>
#include <Common/DenseMap.hpp>
#include <Common/Fundamental.hpp>
#include <Common/IntrusiveRefCntPtr.hpp>
#include <Common/Option.hpp>
#include <Common/Map.hpp>
#include <Common/STLExtras.hpp>
#include <Common/SmallStr.hpp>
#include <Common/SmallVec.hpp>
#include <Common/StrRef.hpp>
#include <Common/StringSet.hpp>
#include <Common/Twine.hpp>
#include <Common/Vec.hpp>
#include <Common/iterator_range.hpp>
#include <Support/Casting.hpp>
#include <Support/Chrono.hpp>
#include <Support/Debug.hpp>
#include <Support/Errc.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/ErrorOr.hpp>
#include <Support/Filesystem.hpp>
#include <Support/FileSystem/UniqueID.hpp>
#include <Support/MemoryBuffer.hpp>
#include <Support/Path.hpp>
// #include <Support/SMLoc.hpp>
#include <Support/raw_ostream.hpp>
#include <atomic>
#include <iterator>
#include <limits>
#include <memory>
#include <system_error>
#include <utility>

using namespace exi;
using namespace exi::vfs;

using exi::sys::fs::file_t;
using exi::sys::fs::file_status;
using exi::sys::fs::file_type;
using exi::sys::fs::kInvalidFile;
using exi::sys::fs::perms;
using exi::sys::fs::UniqueID;

Status::Status(const sys::fs::file_status& Stat) :
 UID(Stat.getUniqueID()), LastModified(Stat.getLastModificationTime()),
 User(Stat.getUser()), Group(Stat.getGroup()), Size(Stat.getSize()),
 Type(Stat.type()), Perms(Stat.permissions()) { }

Status::Status(const Twine& Name, sys::fs::UniqueID UID,
			 sys::TimePoint<> MTime, u32 User, u32 Group, u64 Size,
			 sys::fs::file_type Type, sys::fs::perms Perms) :
 Name(Name.str()), UID(UID), LastModified(MTime),
 User(User), Group(Group), Size(Size), Type(Type), Perms(Perms) { }


Status Status::CopyWithNewSize(const Status& In, u64 NewSize) {
	return Status(In.getName(), In.getUniqueID(), In.getLastModificationTime(),
                In.getUser(), In.getGroup(), NewSize, In.getType(),
                In.getPermissions());
}

Status Status::CopyWithNewName(const Status& In, const Twine& NewName) {
	return Status(NewName, In.getUniqueID(), In.getLastModificationTime(),
                In.getUser(), In.getGroup(), In.getSize(), In.getType(),
                In.getPermissions());
}

Status Status::CopyWithNewName(const sys::fs::file_status& In,
                               const Twine& NewName) {
  return Status(NewName, In.getUniqueID(), In.getLastModificationTime(),
                In.getUser(), In.getGroup(), In.getSize(), In.type(),
                In.permissions());
}

bool Status::equivalent(const Status& Other) const {
  assert(isStatusKnown() && Other.isStatusKnown());
  return getUniqueID() == Other.getUniqueID();
}

bool Status::isDirectory() const { return Type == file_type::directory_file; }

bool Status::isRegularFile() const { return Type == file_type::regular_file; }

bool Status::isOther() const {
  return exists() && !isRegularFile() && !isDirectory() && !isSymlink();
}

bool Status::isSymlink() const { return Type == file_type::symlink_file; }

bool Status::isStatusKnown() const { return Type != file_type::status_error; }

bool Status::exists() const {
  return isStatusKnown() && Type != file_type::file_not_found;
}

//======================================================================//
// File[system]
//======================================================================//

File::~File() = default;
FileSystem::~FileSystem() = default;

ErrorOr<Box<File>>
 File::GetWithPath(ErrorOr<Box<File>> Result, const Twine& P) {
  // Don't update path if it's exposing an external path.
  if (!Result || (*Result)->status()->HasExternalVFSPath)
    return Result;

  ErrorOr<Box<File>> F = std::move(*Result);
  auto Name = F->get()->getName();
  if (Name && Name.get() != P.str())
    F->get()->setPath(P);
  return F;
}

ErrorOr<Box<MemoryBuffer>>
 FileSystem::getBufferForFile(const Twine& Name, i64 FileSize,
                              bool RequiresNullTerminator, bool IsVolatile,
                              bool IsText, bool IsMutable) {
  auto F = IsText ? openFileForRead(Name) : openFileForReadBinary(Name);
  if (!F)
    return F.getError();

  return (*F)->getBuffer(Name, FileSize, RequiresNullTerminator,
                         IsVolatile, IsMutable);
}

std::error_code FileSystem::makeAbsolute(SmallVecImpl<char>& Path) const {
  if (sys::path::is_absolute(Path))
    return {};

  auto WorkingDir = getCurrentWorkingDirectory();
  if (!WorkingDir)
    return WorkingDir.getError();

  sys::fs::make_absolute(WorkingDir.get(), Path);
  return {};
}

std::error_code FileSystem::getRealPath(const Twine& Path,
                                        SmallVecImpl<char>& Output) {
  return errc::operation_not_permitted;
}

std::error_code FileSystem::isLocal(const Twine& Path, bool& Result) {
  return errc::operation_not_permitted;
}

bool FileSystem::exists(const Twine& Path) {
  auto Status = status(Path);
  return Status && Status->exists();
}

ErrorOr<bool> FileSystem::equivalent(const Twine& A, const Twine& B) {
  auto StatusA = status(A);
  if (!StatusA)
    return StatusA.getError();
  auto StatusB = status(B);
  if (!StatusB)
    return StatusB.getError();
  return StatusA->equivalent(*StatusB);
}

#if !defined(NDEBUG) || defined(EXI_ENABLE_DUMP)
void FileSystem::dump() const { print(dbgs(), PrintType::RecursiveContents); }
#endif

#ifndef NDEBUG
static bool isTraversalComponent(StrRef Component) {
  return Component == ".." || Component == ".";
}

static bool pathHasTraversal(StrRef Path) {
  using namespace exi::sys;

  for (StrRef Comp : exi::make_range(path::begin(Path), path::end(Path)))
    if (isTraversalComponent(Comp))
      return true;
  return false;
}
#endif

//======================================================================//
// RealFileSystem
//======================================================================//

template <typename MB>
static ErrorOr<Box<MB>> getRealBufferImpl(
    sys::fs::file_t FD, const Twine& Name, i64 FileSize,
    bool RequiresNullTerminator, bool IsVolatile) {
  exi_assert(FD != kInvalidFile, "cannot get buffer for closed file");
  return MB::getOpenFile(FD, Name, FileSize,
                         RequiresNullTerminator, IsVolatile);
}

namespace {

/// Wrapper around a raw file descriptor.
class RealFile : public File {
  friend class RealFileSystem;

  file_t FD;
  Status S;
  String RealName;

  RealFile(file_t RawFD, StrRef NewName, StrRef NewRealPathName)
      : FD(RawFD), S(NewName, {}, {}, {}, {}, {},
                     sys::fs::file_type::status_error, {}),
        RealName(NewRealPathName.str()) {
    exi_assert(FD != kInvalidFile, "Invalid or inactive file descriptor");
  }

public:
  ~RealFile() override;

  ErrorOr<Status> status() override;
  ErrorOr<String> getName() override;
  ErrorOr<Box<MemoryBuffer>>
   getBuffer(const Twine& Name, i64 FileSize,
             bool RequiresNullTerminator,
             bool IsVolatile, bool IsMutable) override;
    
  std::error_code close() override;
  void setPath(const Twine& Path) override;
};

} // namespace `anonymous`

RealFile::~RealFile() { close(); }

ErrorOr<Status> RealFile::status() {
  exi_assert(FD != kInvalidFile, "cannot stat closed file");
  if (!S.isStatusKnown()) {
    file_status RealStatus;
    if (std::error_code EC = sys::fs::status(FD, RealStatus))
      return EC;
    S = Status::CopyWithNewName(RealStatus, S.getName());
  }
  return S;
}

ErrorOr<String> RealFile::getName() {
  return RealName.empty() ? S.getName().str() : RealName;
}

ErrorOr<Box<MemoryBuffer>>
 RealFile::getBuffer(const Twine& Name, i64 FileSize,
                     bool RequiresNullTerminator,
                     bool IsVolatile, bool IsMutable) {
  if (IsMutable) {
    return getRealBufferImpl<MemoryBuffer>(
      FD, Name, FileSize, RequiresNullTerminator, IsVolatile);
  } else {
    return getRealBufferImpl<WritableMemoryBuffer>(
      FD, Name, FileSize, RequiresNullTerminator, IsVolatile);
  }
}

std::error_code RealFile::close() {
  std::error_code EC = sys::fs::closeFile(FD);
  FD = kInvalidFile;
  return EC;
}

void RealFile::setPath(const Twine& Path) {
  RealName = Path.str();
  if (auto Status = status())
    S = Status.get().CopyWithNewName(Status.get(), Path);
}

namespace {

/// A file system according to your operating system.
/// This may be linked to the process's working directory, or maintain its own.
///
/// Currently, its own working directory is emulated by storing the path and
/// sending absolute paths to sys::fs:: functions.
/// A more principled approach would be to push this down a level, modelling
/// the working dir as an sys::fs::WorkingDir or similar.
/// This would enable the use of openat()-style functions on some platforms.
class RealFileSystem : public FileSystem {
public:
  explicit RealFileSystem(bool LinkCWDToProcess) {
    if (!LinkCWDToProcess) {
      SmallStr<128> PWD, RealPWD;
      if (std::error_code EC = sys::fs::current_path(PWD))
        WD = EC;
      else if (sys::fs::real_path(PWD, RealPWD))
        WD = WorkingDirectory{PWD, PWD};
      else
        WD = WorkingDirectory{PWD, RealPWD};
    }
  }

  ErrorOr<Status> status(const Twine& Path) override;
  ErrorOr<Box<File>> openFileForRead(const Twine& Path) override;
  ErrorOr<Box<File>> openFileForReadBinary(const Twine& Path) override;
  directory_iterator dir_begin(const Twine& Dir, std::error_code& EC) override;

  ErrorOr<String> getCurrentWorkingDirectory() const override;
  std::error_code setCurrentWorkingDirectory(const Twine& Path) override;
  std::error_code isLocal(const Twine& Path, bool& Result) override;
  std::error_code getRealPath(const Twine& Path,
                              SmallVecImpl<char>& Output) override;

protected:
  void printImpl(raw_ostream& OS, PrintType Type,
                 unsigned IndentLevel) const override;

private:
  // If this FS has its own working dir, use it to make Path absolute.
  // The returned twine is safe to use as long as both Storage and Path live.
  Twine adjustPath(const Twine& Path, SmallVecImpl<char>& Storage) const {
    if (!WD || !*WD)
      return Path;
    Path.toVector(Storage);
    sys::fs::make_absolute(WD->get().Resolved, Storage);
    return Storage;
  }

  ErrorOr<Box<File>>
  openFileForReadWithFlags(const Twine& Name, sys::fs::OpenFlags Flags) {
    SmallStr<256> RealName, Storage;
    Expected<file_t> FDOrErr = sys::fs::openNativeFileForRead(
        adjustPath(Name, Storage), Flags,& RealName);
    if (!FDOrErr)
      return errorToErrorCode(FDOrErr.takeError());
    return Box<File>(
        new RealFile(*FDOrErr, Name.str(), RealName.str()));
  }

  struct WorkingDirectory {
    // The current working directory, without symlinks resolved. (echo $PWD).
    SmallStr<128> Specified;
    // The current working directory, with links resolved. (readlink .).
    SmallStr<128> Resolved;
  };
  Option<ErrorOr<WorkingDirectory>> WD;
};

} // namespace `anonymous`

ErrorOr<Status> RealFileSystem::status(const Twine& Path) {
  SmallStr<256> Storage;
  sys::fs::file_status RealStatus;
  if (std::error_code EC =
          sys::fs::status(adjustPath(Path, Storage), RealStatus))
    return EC;
  return Status::CopyWithNewName(RealStatus, Path);
}

ErrorOr<Box<File>> RealFileSystem::openFileForRead(const Twine& Name) {
  return openFileForReadWithFlags(Name, sys::fs::OF_Text);
}

ErrorOr<Box<File>> RealFileSystem::openFileForReadBinary(const Twine& Name) {
  return openFileForReadWithFlags(Name, sys::fs::OF_None);
}

ErrorOr<String> RealFileSystem::getCurrentWorkingDirectory() const {
  if (WD &&* WD)
    return String(WD->get().Specified);
  if (WD)
    return WD->getError();

  SmallStr<128> Dir;
  if (std::error_code EC = sys::fs::current_path(Dir))
    return EC;
  return String(Dir);
}

std::error_code RealFileSystem::setCurrentWorkingDirectory(const Twine& Path) {
  if (!WD)
    return sys::fs::set_current_path(Path);

  SmallStr<128> Absolute, Resolved, Storage;
  adjustPath(Path, Storage).toVector(Absolute);
  bool IsDir;
  if (auto Err = sys::fs::is_directory(Absolute, IsDir))
    return Err;
  if (!IsDir)
    return std::make_error_code(std::errc::not_a_directory);
  if (auto Err = sys::fs::real_path(Absolute, Resolved))
    return Err;
  WD = WorkingDirectory{Absolute, Resolved};
  return std::error_code();
}

std::error_code RealFileSystem::isLocal(const Twine& Path, bool& Result) {
  SmallStr<256> Storage;
  return sys::fs::is_local(adjustPath(Path, Storage), Result);
}

std::error_code RealFileSystem::getRealPath(const Twine& Path,
                                            SmallVecImpl<char>& Output) {
  SmallStr<256> Storage;
  return sys::fs::real_path(adjustPath(Path, Storage), Output);
}

void RealFileSystem::printImpl(raw_ostream& OS, PrintType Type,
                               unsigned IndentLevel) const {
  printIndent(OS, IndentLevel);
  OS << "RealFileSystem using ";
  if (WD)
    OS << "own";
  else
    OS << "process";
  OS << " CWD\n";
}

IntrusiveRefCntPtr<FileSystem> vfs::getRealFileSystem() {
  static IntrusiveRefCntPtr<FileSystem> FS(new RealFileSystem(true));
  return FS;
}

Box<FileSystem> vfs::createPhysicalFileSystem() {
  return std::make_unique<RealFileSystem>(false);
}

namespace {

class RealFSDirIter : public vfs::H::DirIterImpl {
  sys::fs::directory_iterator Iter;

public:
  RealFSDirIter(const Twine& Path, std::error_code& EC) : Iter(Path, EC) {
    if (Iter != sys::fs::directory_iterator())
      CurrentEntry = directory_entry(Iter->path(), Iter->type());
  }

  std::error_code increment() override {
    std::error_code EC;
    Iter.increment(EC);
    CurrentEntry = (Iter == sys::fs::directory_iterator())
                       		? directory_entry()
                       		: directory_entry(Iter->path(), Iter->type());
    return EC;
  }
};

} // namespace `anonymous`

directory_iterator RealFileSystem::dir_begin(const Twine &Dir,
                                             std::error_code &EC) {
  SmallStr<128> Storage;
  return directory_iterator(
      std::make_shared<RealFSDirIter>(adjustPath(Dir, Storage), EC));
}

//======================================================================//
// Miscellaneous
//======================================================================//

vfs::H::DirIterImpl::~DirIterImpl() = default;

vfs::recursive_directory_iterator::recursive_directory_iterator(
 FileSystem &InFS, const Twine &Path, std::error_code &EC) : FS(&InFS) {
  directory_iterator I = FS->dir_begin(Path, EC);
  if (I != directory_iterator()) {
    State = std::make_shared<H::RecDirIterState>();
    State->Stack.push_back(I);
  }
}

vfs::recursive_directory_iterator&
 recursive_directory_iterator::increment(std::error_code& EC) {
  exi_assert(FS && State && !State->Stack.empty(), "incrementing past end");
  exi_assert(!State->Stack.back()->path().empty(), "non-canonical end iterator");
  vfs::directory_iterator End;

  if (State->HasNoPushRequest)
    State->HasNoPushRequest = false;
  else {
    if (State->Stack.back()->type() == sys::fs::file_type::directory_file) {
      vfs::directory_iterator I =
          FS->dir_begin(State->Stack.back()->path(), EC);
      if (I != End) {
        State->Stack.push_back(I);
        return *this;
      }
    }
  }

  while (!State->Stack.empty() && State->Stack.back().increment(EC) == End)
    State->Stack.pop_back();

  if (State->Stack.empty())
    State.reset(); // end iterator

  return *this;
}

const char FileSystem::ID = 0;
