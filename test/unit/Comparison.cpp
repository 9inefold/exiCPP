//===- unit/Comparison.cpp ------------------------------------------===//
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



#include "Testing.hpp"

#include <exicpp/BinaryBuffer.hpp>
#include <exicpp/Filesystem.hpp>
#include <exicpp/Options.hpp>
#include <exicpp/Writer.hpp>
#include <exicpp/XML.hpp>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <STL.hpp>

#include <concepts>
#include <cstdlib>
#include <utility>

using namespace exi;

namespace {

template <typename...TT>
using TestWithParams = testing::TestWithParam<std::tuple<TT...>>;

using String = std::string;

//////////////////////////////////////////////////////////////////////////
// Tuple Generator

template <typename T, std::size_t = 0>
using NType = T;

template <typename, class>
struct TTupleGenerator;

template <typename T, std::size_t...NN>
struct TTupleGenerator<T, std::index_sequence<NN...>> {
  using type = std::tuple<NType<T, NN>...>;
};

template <typename T, std::size_t N>
using FillTuple = typename
  TTupleGenerator<T, std::make_index_sequence<N>>::type;

//////////////////////////////////////////////////////////////////////////
// is_tuple

template <typename...TT>
std::true_type iis_tuple(const std::tuple<TT...>&);

template <typename T>
std::false_type iis_tuple(T&&);

template <typename T>
using IIsTuple = decltype(iis_tuple(std::declval<T>()));

template <typename T>
concept is_tuple = IIsTuple<T>::value;

//======================================================================//
// Test Utils
//======================================================================//

#ifdef WIN32
static constexpr StrRef shell_eat = "nul";
#else
static constexpr StrRef shell_eat = "/dev/null";
#endif

struct System {
  String formatCall(const String& S) const;
  int call(const String& S, bool do_flush = true) const;
  int callExificent(const Vec<String>& args, bool do_flush = true) const;
  int callExificent(const String& arg = "", bool do_flush = true) const;

  int operator()(const String& S, bool do_flush = true) const {
    return this->call(S, do_flush);
  }

public:
  StrRef out = "";
  StrRef err = "";
};

String System::formatCall(const String& S) const {
  if (out.empty()) {
    if (err.empty())
      return S;
    else
      return fmt::format("{} 2>{}", S, err);
  } else {
    if (err.empty())
      return fmt::format("{} >{}", S, out);
    else
      return fmt::format("{} >{} 2>{}", S, out, err);
  }
}

int System::call(const String& S, bool do_flush) const {
  if (do_flush)
    std::flush(std::cout);
  String cmd = this->formatCall(S);
  // fmt::println(stderr, "Exec: '{}'", cmd);
  return std::system(cmd.c_str());
}

int System::callExificent(const Vec<String>& args, bool do_flush) const {
  auto cmd = fmt::format("java -jar {} {}",
    exificent,
    fmt::join(args, " ")
  );
  return this->call(cmd, do_flush);
}

int System::callExificent(const String& arg, bool do_flush) const {
  // Explicitly passing the allocator to avoid an ICE pre gcc 12.0
  using A = typename Vec<String>::allocator_type;
  return this->callExificent(Vec({arg}, A{}), do_flush);
}

//////////////////////////////////////////////////////////////////////////
// Predefined

namespace shell {
  /// Eat both `stdout` and `stderr`.
  static constexpr System None {shell_eat, shell_eat};
  /// Eat only `stdout`, let `stderr` print.
  static constexpr System Err  {shell_eat, ""};
  /// Allow both `stdout` and `stderr` to print.
  static constexpr System All  {"", ""};
} // namespace shell

//======================================================================//
// Options
//======================================================================//

using OptsType      = bool;
using OptsTuple     = FillTuple<OptsType, 12>;
using OptsTupleSlim = FillTuple<OptsType, 7>;

struct Opts {
  String getName() const;
  exi::Options forExip() const;
  Vec<String> forExificent() const;
public:
  OptsType Compression:           1 = false;
  OptsType Strict:                1 = false;
  OptsType Fragment:              1 = false;
  OptsType SelfContained:         1 = false;

  OptsType BitPacked:             1 = true;
  OptsType ByteAligned:           1 = false;
  OptsType PreCompression:        1 = false;

  OptsType PreserveComments:      1 = false;
  OptsType PreservePIs:           1 = false;
  OptsType PreserveDTD:           1 = false;
  OptsType PreservePrefixes:      1 = false;
  OptsType PreserveLexicalValues: 1 = false;
};

struct OptsProxy : public Opts {
  OptsProxy() = default;
  OptsProxy(const OptsProxy&) = default;
  OptsProxy(OptsProxy&&) = default;

  constexpr OptsProxy(const OptsTuple& tup) :
   Opts{std::make_from_tuple<Opts>(tup)} {
  }

  constexpr OptsProxy(const OptsTupleSlim& tup) : OptsProxy() {
    auto [c, bit, byte, pc, com, pre, lv] = tup;
    this->Compression           = c;
    this->BitPacked             = bit;
    this->ByteAligned           = byte;
    this->PreCompression        = pc;
    this->PreserveComments      = com;
    this->PreservePrefixes      = pre;
    this->PreserveLexicalValues = lv;
  }
};

#define SET_OPT(flag, bl, val) do { \
  if (!!this->flag) { \
    O.append(val); \
    bl = true; \
  } \
} while(0)

String Opts::getName() const {
  String O;
  
  bool opt = false;
  O.append("Opt");
  SET_OPT(Compression,            opt, "C");
  SET_OPT(Strict,                 opt, "S");
  SET_OPT(Fragment,               opt, "F");
  SET_OPT(SelfContained,          opt, "I");
  if (!opt) O.append("None");
  O.push_back('_');

  [[maybe_unused]] bool align = false;
  O.append("Al");
  SET_OPT(ByteAligned,            align, "Byte");
  SET_OPT(BitPacked,              align, "Bit");  
  SET_OPT(PreCompression,         align, "Pre");
  O.push_back('_');

  bool preserve = false;
  O.append("Ps");
  SET_OPT(PreserveComments,       preserve, "C");
  SET_OPT(PreservePIs,            preserve, "I");
  SET_OPT(PreserveDTD,            preserve, "D");
  SET_OPT(PreservePrefixes,       preserve, "P");
  SET_OPT(PreserveLexicalValues,  preserve, "L");
  if (!preserve) O.append("None");

#undef SET_OPT
  return O;
}

#define SET_OPT(flag, val) do { \
  if (!!this->flag) O.set(val); \
} while(0)

exi::Options Opts::forExip() const {
  exi::Options O {};

  SET_OPT(Compression,            EnumOpt::Compression);
  SET_OPT(Strict,                 EnumOpt::Strict);
  SET_OPT(Fragment,               EnumOpt::Fragment);
  SET_OPT(SelfContained,          EnumOpt::SelfContained);

  SET_OPT(ByteAligned,            Align::ByteAlignment);
  SET_OPT(PreCompression,         Align::PreCompression);
  SET_OPT(BitPacked,              Align::BitPacked);

  SET_OPT(PreserveComments,       Preserve::Comments);
  SET_OPT(PreservePIs,            Preserve::PIs);
  SET_OPT(PreserveDTD,            Preserve::DTD);
  SET_OPT(PreservePrefixes,       Preserve::Prefixes);
  SET_OPT(PreserveLexicalValues,  Preserve::LexicalValues);

#undef SET_OPT
  return O;
}

#define SET_OPT(flag, val) do { \
  if (!!this->flag) O.emplace_back(val); \
} while(0)

Vec<String> Opts::forExificent() const {
  Vec<String> O;
  O.reserve(4);
  O.emplace_back("-includeOptions");

  SET_OPT(Compression,            "-compression");
  SET_OPT(Strict,                 "-strict");
  SET_OPT(Fragment,               "-fragment");
  // SET_OPT(SelfContained,          "-selfContained");

  if (!this->BitPacked)
    SET_OPT(ByteAligned,          "-bytePacked");
  SET_OPT(PreCompression,         "-preCompression");

  SET_OPT(PreserveComments,       "-preserveComments");
  SET_OPT(PreservePIs,            "-preservePIs");
  SET_OPT(PreserveDTD,            "-preserveDTDs");
  SET_OPT(PreservePrefixes,       "-preservePrefixes");
  SET_OPT(PreserveLexicalValues,  "-preserveLexicalValues");

#undef SET_OPT
  return O;
}

//======================================================================//
// Suite
//======================================================================//

class CompareExi : public testing::TestWithParam<OptsProxy> {
protected:
  void SetUp() override {
    static bool dir_exists = fs::exists(exificent_dir);
    if (!dir_exists)
      GTEST_SKIP() << "Exificent directory could not be found. Skipping.";
    
    static bool file_exists = fs::exists(exificent);
    if (!file_exists)
      GTEST_SKIP() << "Exificent could not be found. Skipping.";
    
    static bool java_exists = CheckJavaInstall();
    if (!java_exists) {
      fmt::println(stderr, "Ensure Java has been installed!");
      GTEST_SKIP() << "Java could not be found. Skipping.";
    }
  }

  static bool CheckJavaInstall();
};

bool CompareExi::CheckJavaInstall() {
  // These are on both UNIX and modern Windows.
  int ret = shell::Err.call("which java", false);
#ifdef _WIN32
  if (ret != 0) {
    // If invalid for any reason, check where.
    ret = shell::Err.call("where /q java", false);
  }
#endif
  return (ret == 0);
}

//////////////////////////////////////////////////////////////////////////
// Testing

std::pair<fs::path, fs::path> getValidXMLPaths() {
  fs::path curr = get_test_dir();
  assert(curr.stem() == "test");
  fs::path dir = curr / "xmltest/valid/sa";
  assert(fs::exists(dir));
  return {dir, dir / "out"};
}

Option<fs::path> checkXMLTest(const fs::directory_entry& entry) {
  if (!entry.is_regular_file())
    return std::nullopt;
  fs::path file = entry.path();
  if (file.extension() != ".xml")
    return std::nullopt;
  return {std::move(file)};
  // return {fs::absolute(file)};
}

fs::path replaceExt(fs::path path, const fs::path& ext) {
  return path.replace_extension(ext);
}

TEST_P(CompareExi, Encode) {
  Opts O = GetParam();
  exi::Options exip_opts = O.forExip();
  Vec<String> exif_opts = O.forExificent();
  HeapBuffer bufBase {(2048 * 32) - 1};

  auto tmp = get_test_dir() / "tmp" / O.getName();
  auto [in, out] = getValidXMLPaths();
  (void) fs::create_directories(tmp);

  exif_opts.reserve(exif_opts.size() + 5);
  for (const auto& entry : fs::directory_iterator{in}) {
    Option<fs::path> file = checkXMLTest(entry);
    if (!file)
      continue;

    auto exip = replaceExt(tmp / file->filename(), ".exip.exi");
    BinaryBuffer buf {bufBase};
    auto xmldoc = BoundDocument::ParseFrom(*file);
    if (!xmldoc) {
      ADD_FAILURE() << "XML parse error in: " << (*file);
      continue;
    }
    if (Error E = buf.writeFile(exip)) {
      ADD_FAILURE()
        << "exip error in: " << (*file)
        << "; " << E.message();
      continue;
    }
    if (Error E = write_xml(xmldoc.document(), buf, exip_opts)) {
      ADD_FAILURE() << "Invalid exip input: " << (*file);
      continue;
    }
    
    auto exif = replaceExt(tmp / file->filename(), ".exif.exi");
    ResizeAdaptor O(exif_opts);
    O->emplace_back("-encode");
    O->emplace_back("-i");
    O->emplace_back(to_multibyte(*file).c_str());
    O->emplace_back("-o");
    O->emplace_back(to_multibyte(exif).c_str());
    int ret = shell::All.callExificent(O.get());
    EXPECT_EQ(ret, 0) << "Invalid exif input: " << (*file);
  }
}

//////////////////////////////////////////////////////////////////////////
// Instance

using ::testing::Bool;
using TestTuple = OptsTupleSlim; 
using TestType  = testing::TestParamInfo<CompareExi::ParamType>;

auto getName(const TestType& I) -> std::string {
  const Opts& O = I.param;
  return O.getName();
};

INSTANTIATE_TEST_SUITE_P(Conformance, CompareExi,
  testing::ConvertGenerator<TestTuple>(
    testing::Combine(
      Bool(), Bool(), Bool(), Bool(),
      Bool(), Bool(), Bool()
      // , Bool(), Bool(), Bool(), Bool(), Bool()
    )
  ),
  &getName
);

} // namespace `anonymous`
