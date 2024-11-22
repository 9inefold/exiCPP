//===- Debug/Terminal.hpp -------------------------------------------===//
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

#include <exicpp/Debug/Terminal.hpp>
#include <exicpp/Errors.hpp>
#ifdef _WIN32
# include <io.h>
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# undef TRUE
# undef FALSE
#else
# include <sys/stat.h>
# include <unistd.h>
#endif

namespace exi {
namespace dbg {

bool isAtty(int fd) {
#ifdef _WIN32
  return _isatty(fd);
#else
  return ::isatty(fd);
#endif
}

int fileNo(std::FILE* stream) {
#ifdef _WIN32
  return _fileno(stream);
#else
  return ::fileno(stream);
#endif
}

bool can_use_ansi(bool refresh) {
  static bool B = !!exip::exipCanUseAnsi();
  if EXICPP_UNLIKELY(refresh) {
    // Run everything again. Useful if attaching new consoles.
    B = !!exip::exipCanUseAnsi(exip::TRUE);
  }
  return B;
}

} // namespace dbg
} // namespace exi
