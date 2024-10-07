//===- XML.cpp ------------------------------------------------------===//
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

#include <XML.hpp>
#include <Strings.hpp>
#include <Debug/Format.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace exi;
namespace fs = std::filesystem;

using Str = std::string;
using BoxedStr = std::unique_ptr<Str>;
using Traits = std::char_traits<char>;

static BoxedStr read_file(fs::path filepath) {
  if (filepath.is_relative()) {
    filepath = (fs::current_path() / filepath);
  }
  std::ifstream is (filepath, std::ios::binary);
  is.unsetf(std::ios::skipws);
  if (!is)
    return BoxedStr();

  std::streampos size;
  is.seekg(0, std::ios::end);
  size = is.tellg();
  is.seekg(0, std::ios::beg);

  Str file_data;
  std::istream_iterator<char> start(is), end;
  file_data.reserve(size);
  file_data.insert(file_data.cbegin(), start, end);
  return std::make_unique<Str>(std::move(file_data));
}

BoundDocument BoundDocument::From(fs::path filename) {
  BoundDocument res;
  auto str = read_file(filename);
  if (!str || str->empty()) {
#if EXICPP_DEBUG
    auto mbstr = to_multibyte(filename.native());
    LOG_ERROR("Could not read file '{}'", mbstr);
#endif
    return res;
  }

  auto& buf = res.buf;
  const std::size_t len = str->size();
  buf.set(len + 1);
  Traits::copy(buf.data(), str->data(), len);
  return res;
}


// TODO: MOVE
namespace exi {

std::string to_multibyte(const std::string& str) {
  return str;
}

std::string to_multibyte(const std::wstring& str) {
  std::string outstr;
  const std::size_t wsize = str.size() + 1;
  outstr.resize(wsize * 2);
  const auto len = std::wcstombs(outstr.data(), str.data(), outstr.size());
  if (len == static_cast<std::size_t>(-1))
    return "...";
  outstr.resize(len);
  return outstr;
}

} // namespace exi
