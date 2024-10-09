//===- Debug/Format.cpp ---------------------------------------------===//
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

#include <Debug/Format.hpp>
#include <cstdlib>
#include <fmt/format.h>
#include <fmt/color.h>

#define FG(col) fmt::fg(fmt::color::col)
#define BG(col) fmt::bg(fmt::color::col)

#if EXICPP_ANSI
# define STYLED(msg, col) fmt::styled(msg, col)
#else
# define STYLED(msg, col) (msg)
#endif

using namespace exi;

#ifdef _WIN32
  static constexpr char folderDelim = '\\';
#else
  static constexpr char folderDelim = '/';
#endif

static auto formatFunc(const dbg::Location& loc) {
  return fmt::format("'{}'", loc.func);
}

static AsciiStrRef sliceFilename(AsciiStrRef file) {
  std::size_t startPos = AsciiStrRef::npos;
  int hitCount = 0;
  std::size_t pos = 0;
  
  for (int hitCount = 0; hitCount < dbg::filenameDepth; ++hitCount) {
    pos = file.find_last_of(folderDelim, startPos);
    if (!pos || pos == AsciiStrRef::npos)
      break;
    startPos = pos - 1;
  }

  if (startPos == AsciiStrRef::npos)
    return file;
  return file.substr(startPos + 2);
}

static auto formatFileLoc(const dbg::Location& loc) {
  auto [file, func, line, col] = loc;
  auto filename = sliceFilename(file);
  if constexpr (dbg::Location::HasColumn())
    return fmt::format("[\"{}\":{}:{}]", filename, line, col);
  else
    return fmt::format("[\"{}\":{}]", filename, line);
}

static auto getFgColor(int debugLevel) {
  switch (debugLevel) {
   case INFO:
    return FG(light_green);
   case WARNING:
    return FG(yellow);
   case ERROR:
    return fmt::fg(fmt::terminal_color::bright_red);
   case FATAL:
    return fmt::fg(fmt::terminal_color::red);
   default:
    return FG(light_gray);
  }
}

ALWAYS_INLINE static void handleFatal(int debugLevel) {
  if EXICPP_UNLIKELY(debugLevel == FATAL) {
    dbg::fatalError();
  }
}

namespace exi {
namespace dbg {

template <> void logInternal<true>(
  const dbg::Location& loc,
  const std::string& msg,
  int debugLevel)
{
  if (DEBUG_GET_MODE() || debugLevel == FATAL) {
    fmt::println("In {} {}: {}",
      STYLED(formatFunc(loc), FG(aquamarine)),
      STYLED(formatFileLoc(loc), FG(aqua)),
      STYLED(msg, getFgColor(debugLevel))
    );
  }
  handleFatal(debugLevel);
}

template <> void logInternal<false>(
  const dbg::Location& loc,
  const std::string& msg,
  int debugLevel)
{
  if (DEBUG_GET_MODE() || debugLevel == FATAL) {
    fmt::println("In {}: {}",
      STYLED(formatFunc(loc), FG(aquamarine)),
      STYLED(msg, getFgColor(debugLevel))
    );
  }
  handleFatal(debugLevel);
}

[[noreturn]] void fatalError() {
  std::fflush(stdout);
  std::abort();
}

} // namespace dbg
} // namespace exi
