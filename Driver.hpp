//===- Driver.hpp ---------------------------------------------------===//
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

#pragma once

#include <exicpp/Strings.hpp>
#include <exicpp/Debug/Format.hpp>
#include <filesystem>

namespace fs = std::filesystem;

struct ArgProcessor {
  struct It {
    using difference_type = std::ptrdiff_t;
    using value_type = exi::StrRef;
  public:
    It() = default;
    It(char** data) : data_(data), len(exi::strsize(*data)) {}
    char* data() const { return *this->data_; }
    std::size_t size() const { return this->len; }
  public:
    exi::StrRef operator*() const {
      if (this->data_)
        return {this->data(), this->len};
      return "";
    }

    It& operator++() {
      this->next();
      return *this;
    }
 
    It operator++(int) {
      auto tmp = *this;
      ++*this;
      return tmp;
    }
 
    bool operator==(const It& rhs) const {
      return (this->data_ == rhs.data_);
    }

    bool operator!=(const It& rhs) const {
      return (this->data_ != rhs.data_);
    }
  
  private:
    void next() {
      ++this->data_;
      this->len = exi::strsize(*data_);
    }

  private:
    char** data_ = nullptr;
    std::size_t len = 0;
  };

public:
  ArgProcessor(int argc, char* argv[]) :
   argc(argc - 1), argv(argv + 1) {
    LOG_ASSERT(argc && argv);
  }

public:
  [[nodiscard]] exi::StrRef curr() const {
    return {*argv, exi::strsize(*argv)};
  }

  [[nodiscard]] exi::StrRef peek() const {
    if (*argv)
      return {argv[1], exi::strsize(argv[1])};
    return "";
  }

  bool next() {
    if (*argv == nullptr)
      return false;
    --this->argc;
    ++this->argv;
    return true;
  }

  explicit operator bool() const {
    if EXICPP_UNLIKELY(!argv)
      return false;
    return *argv;
  }

public:
  It begin() const { return {argv}; }
  It end() const { return {argv + argc}; }

private:
  int argc = 0;
  char** argv = nullptr;
};

enum Mode {
  Default,
  Help = Default,
  Encode,
  Decode,
  EncodeDecode,
};

template <>
class fmt::formatter<fs::path, char> : public fmt::formatter<std::string> {
  using BaseType = fmt::formatter<std::string>;
public:
  template <typename FormatContext>
  FMT_CONSTEXPR auto format(const fs::path& value, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    auto val = exi::to_multibyte(value);
    return BaseType::format(val, ctx);
  }
};
