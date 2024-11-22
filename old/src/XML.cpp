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
#include <Filesystem.hpp>
#include <Debug/Format.hpp>
#include <fstream>
#include <iterator>
#include <string>

using namespace exi;
using Traits = std::char_traits<char>;

static Box<Str> read_file_nolf(std::ifstream& is) {
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

static Box<Str> read_file_lf(std::ifstream& is) {
  std::streampos size;
  is.seekg(0, std::ios::end);
  size = is.tellg();
  is.seekg(0, std::ios::beg);

  Str file_data;
  std::ifstream::sentry se(is, true);
  std::streambuf* sb = is.rdbuf();
  file_data.reserve(size);
  
  while (!is.eof()) {
    const int C = sb->sbumpc();
    switch (C) {
     case '\r': {
      if(sb->sgetc() == '\n') {
        sb->sbumpc();
        file_data.push_back('\n');
      }
      break;
     }
     case std::streambuf::traits_type::eof(): {
      is.setstate(std::ios::eofbit);
      break;
     }
     default:
      file_data.push_back(char(C));
    }
  }

  return std::make_unique<Str>(std::move(file_data));
}

static Box<Str> read_file(fs::path filepath, bool norm_lf) {
  if (filepath.is_relative()) {
    filepath = (fs::current_path() / filepath);
  }
  std::ifstream is (filepath, std::ios::binary);
  is.unsetf(std::ios::skipws);
  if (!is)
    return Box<Str>();
  
  if (!norm_lf)
    return read_file_nolf(is);
  else
    return read_file_lf(is);
}

BoundDocument BoundDocument::From(const fs::path& filename, bool norm_lf) {
  BoundDocument res;
  Box<Str> str = read_file(filename, norm_lf);
  if (!str) {
#if EXICPP_DEBUG
    auto mbstr = to_multibyte(filename.native());
    LOG_ERROR("Could not read file '{}'", mbstr);
#endif
    return res;
  } else if (str->empty()) {
#if EXICPP_DEBUG
    auto mbstr = to_multibyte(filename.native());
    LOG_ERROR("'{}' is empty", mbstr);
#endif
    return res;
  } else if (str->size() >= 4) {
    StrRef maybeMarker{str->data(), 4};
    if (maybeMarker == "$EXI") {
      auto mbstr = to_multibyte(filename.native());
      LOG_ERROR("'{}' is an exi file", mbstr);
      return res;
    }
  }

  auto& buf = res.buf;
  const std::size_t len = str->size();
  buf.set(len + 1);
  Traits::copy(buf.data(), str->data(), len);
  return res;
}

bool exi::set_xml_allocators(XMLDocument* doc) {
  if (!doc)
    return false;
  doc->set_allocator(&mi_malloc, &mi_free);
  return true;
}

void BoundDocument::setAllocators() {
#if EXIP_USE_MIMALLOC
  const bool did_set_allocators
    = set_xml_allocators(this->document());
  LOG_ASSERT(did_set_allocators);
#endif
}

void BoundDocument::LogException(const std::exception& e) {
#if EXICPP_DEBUG
  fmt::println("[ERROR] {}",
    dbg::styled(e.what(),
      fmt::fg(fmt::terminal_color::red)
    )
  );
#endif
}
