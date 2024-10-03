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
#include <fmt/format.h>
#include <fmt/color.h>

#define FG(col) fmt::fg(fmt::color::col)
#define BG(col) fmt::bg(fmt::color::col)

using namespace exi;

static auto formatFunc(const dbg::Location& loc) {
  return fmt::format("'{}'", loc.func);
}

static StrRef sliceFilename(StrRef file) {
  std::size_t pos = file.find_last_of("\\/");
  if (pos == exi::StrRef::npos)
    return file;
  return file.substr(pos + 1);
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
    return FG(red);
   default:
    return FG(light_gray);
  }
}

namespace exi {
namespace dbg {

template <> void logInternal<true>(
  const dbg::Location& loc,
  const std::string& msg,
  int debugLevel)
{
  fmt::println("In {} {}: {}",
    fmt::styled(formatFunc(loc), FG(aquamarine)),
    fmt::styled(formatFileLoc(loc), FG(aqua)),
    fmt::styled(msg, getFgColor(debugLevel))
  );
}

template <> void logInternal<false>(
  const dbg::Location& loc,
  const std::string& msg,
  int debugLevel)
{
  fmt::println("In {}: {}",
    fmt::styled(formatFunc(loc), FG(aquamarine)),
    fmt::styled(msg, getFgColor(debugLevel))
  );
}

} // namespace dbg
} // namespace exi
