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
#include <rapidxml_print.hpp>

#include <CompareXml.hpp>
#include <ExiToXml.hpp>
#include <STL.hpp>

#include <array>
#include <concepts>
#include <cstdlib>
#include <utility>
#include <fmt/format.h>
#include <fmt/ranges.h>

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

using OptsType = std::uint8_t;

enum AlignType : std::uint8_t {
  AL_BitPacked,
  AL_BytePacked,
  AL_PreCompression,
  AL_Compression,
};

using OptsTuple = std::tuple<
  bool, // Strict
  bool, // Fragment
  bool, // SelfContained
  AlignType,
  bool, // PreserveComments
  bool, // PreservePIs
  bool, // PreserveDTD
  bool, // PreservePrefixes
  bool  // PreserveLexicalValues
>;

/// Has the values `[AlignType, PComments, PPrefixes, PLexVals]`.
using OptsTupleSlim = std::tuple<
  AlignType,
  bool, // PreserveComments
  bool, // PreservePrefixes
  bool  // PreserveLexicalValues
>;

/// A helper type, generates options from a generic interface.
struct Opts {
  String getName() const;
  exi::Options forExip() const;
  Vec<String> forExificent() const;
public:
  OptsType Strict:                1 = false;
  OptsType Fragment:              1 = false;
  OptsType SelfContained:         1 = false;

  OptsType Alignment:             3 = AL_BitPacked;

  OptsType PreserveComments:      1 = false;
  OptsType PreservePIs:           1 = false;
  OptsType PreserveDTD:           1 = false;
  OptsType PreservePrefixes:      1 = false;
  OptsType PreserveLexicalValues: 1 = false;
};

/// Used to construct `Opts` from tuples.
struct OptsProxy : public Opts {
  OptsProxy() = default;
  OptsProxy(const OptsProxy&) = default;
  OptsProxy(OptsProxy&&) = default;

  constexpr OptsProxy(const OptsTuple& tup) :
   Opts{std::make_from_tuple<Opts>(tup)} {
  }

  constexpr OptsProxy(const OptsTupleSlim& tup) : OptsProxy() {
    auto [al, com, pre, lv] = tup;
    this->Alignment             = al;
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
  SET_OPT(Strict,                 opt, "S");
  SET_OPT(Fragment,               opt, "F");
  SET_OPT(SelfContained,          opt, "C");
  if (!opt) O.append("N");
  O.push_back('_');

  O.append("Al");
  if (this->Alignment == AL_BitPacked) {
    O.append("Bit");
  } else if (this->Alignment == AL_BytePacked) {
    O.append("Byte");
  } else if (this->Alignment == AL_Compression) {
    O.append("Com");
  } else {
    O.append("Pre");
  }
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

  SET_OPT(Strict,                 EnumOpt::Strict);
  SET_OPT(Fragment,               EnumOpt::Fragment);
  SET_OPT(SelfContained,          EnumOpt::SelfContained);

  if (this->Alignment == AL_BitPacked) {
    O.set(Align::BitPacked);
  } else if (this->Alignment == AL_BytePacked) {
    O.set(Align::BytePacked);
  } else if (this->Alignment == AL_Compression) {
    O.set(EnumOpt::Compression);
  } else {
    O.set(Align::PreCompression);
  }

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
  O.reserve(8);
  O.emplace_back("-includeOptions");
  O.emplace_back("-includeCookie");

  SET_OPT(Strict,                 "-strict");
  SET_OPT(Fragment,               "-fragment");
  SET_OPT(SelfContained,          "-selfContained");

  if (this->Alignment == AL_BitPacked) {
    // Implicit:
    // O.emplace_back("-bitPacked");
  } else if (this->Alignment == AL_BytePacked) {
    O.emplace_back("-bytePacked");
  } else if (this->Alignment == AL_Compression) {
    O.emplace_back("-compression");
  } else {
    O.emplace_back("-preCompression");
  }

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
  if (file.stem() == "012") {
    fmt::println("Skipping 012.xml");
    return std::nullopt;
  }
  return {std::move(file)};
  // return {fs::absolute(file)};
}

fs::path replaceExt(fs::path path, const fs::path& ext) {
  return path.replace_extension(ext);
}

bool removeFile(const fs::path& path) {
  std::error_code ec;
  (void) fs::remove(path, ec);
  if (ec) {
    fmt::println(stderr, "File error: {}", ec.message());
    return false;
  }
  return true;
}

std::uintmax_t getFileSize(const fs::path& path) {
  std::error_code ec;
  const auto ret = fs::file_size(path, ec);
  if (ec) {
    fmt::println(stderr, "File error: {}", ec.message());
    return 0;
  }
  return ret;
}

#define CONTINUE(count) { \
  if ((count)++ >= error_max) { break; } \
  continue; } (void(0))
#define CONTINUE_FAIL() CONTINUE(failure_count)

constexpr std::size_t error_max = 16;

TEST_P(CompareExi, Encode) {
  Opts O = GetParam();
  exi::Options exip_opts = O.forExip().set(Preserve::Prefixes);
  Vec<String> exif_opts = O.forExificent();

  const CompareOpts cmp_opts {
    .preserveComments = !!O.PreserveComments,
    .preservePIs      = !!O.PreservePIs,
    .preserveDTs      = !!O.PreserveDTD,
    .verbose          = false
  };

  auto tmp = get_test_dir() / "tmp" / O.getName();
  auto [in, out] = getValidXMLPaths();
  (void) fs::create_directories(tmp);

  HeapBuffer bufBase {(2048 * 32) - 1};
  std::size_t failure_count = 0;
  exif_opts.reserve(exif_opts.size() + 6);
  for (const auto& entry : fs::directory_iterator{out}) {
    Option<fs::path> file = checkXMLTest(entry);
    if (!file)
      continue;

    //////////////////////////////////////////////////////////////////////
    // ExiCPP Encode

    auto exip = replaceExt(tmp / file->filename(), ".exip.exi");
    auto xmldoc = BoundDocument::ParseFromEx<0, false>(*file);
    if (!xmldoc) {
      ADD_FAILURE() << "XML parse error in: " << (*file);
      CONTINUE_FAIL();
    }
    {
      BinaryBuffer buf {bufBase};
      if (Error E = buf.writeFile(exip)) {
        ADD_FAILURE()
          << "exip error in: " << (*file)
          << "; " << E.message();
        CONTINUE_FAIL();
      }
      if (Error E = write_xml(xmldoc.document(), buf, exip_opts, true)) {
        ADD_FAILURE() << "Invalid exip input: " << (*file);
        CONTINUE_FAIL();
      }
    }

    //////////////////////////////////////////////////////////////////////
    // Exificent Encode

    auto exif = replaceExt(tmp / file->filename(), ".exif.exi");
    {
      removeFile(exif);
      ResizeAdaptor O(exif_opts);
      O->emplace_back("-encode");
      O->emplace_back("-noSchema");
      O->emplace_back("-i");
      O->emplace_back(to_multibyte(*file).c_str());
      O->emplace_back("-o");
      O->emplace_back(to_multibyte(exif).c_str());
      int ret = shell::All.callExificent(O.get());

      EXPECT_EQ(ret, 0) << "Invalid exif input: " << (*file);
      if (!fs::exists(exif)) {
        if (ret == 0)
          ADD_FAILURE() << "exif unable to encode: " << (*file);
        CONTINUE_FAIL();
      }
    }

    //////////////////////////////////////////////////////////////////////
    // ExiCPP Decode

    auto exif_doc = exi_to_xml(exif);
    if EXICPP_UNLIKELY(!exif_doc) {
      ADD_FAILURE() << "exip failed to decode: " << exif;
      CONTINUE_FAIL();
    }

    //////////////////////////////////////////////////////////////////////
    // Exificent Decode

    auto exip_xml = replaceExt(exip, "xml");
    {
      removeFile(exip_xml);
      // ResizeAdaptor O(exif_opts);
      // O->emplace_back("-decode");
      // O->emplace_back("-noSchema");
      // O->emplace_back("-i");
      // O->emplace_back(to_multibyte(exip).c_str());
      // O->emplace_back("-o");
      // O->emplace_back(to_multibyte(exip_xml).c_str());
      // int ret = shell::All.callExificent(O.get());
      String cmd = fmt::format("-decode -noSchema -i \"{}\" -o \"{}\"",
        to_multibyte(exip).c_str(),
        to_multibyte(exip_xml).c_str()
      );
      int ret = shell::All.callExificent(cmd);

      EXPECT_EQ(ret, 0) << "Invalid exif input: " << exip;
      if (!fs::exists(exip_xml)) {
        if (ret == 0)
          ADD_FAILURE() << "exif unable to decode: " << exip;
        CONTINUE_FAIL();
      }
    }

    auto exip_doc = BoundDocument::ParseFromEx<
      rapidxml::parse_no_data_nodes
    >(exip_xml, true);
    if EXICPP_UNLIKELY(!exip_doc) {
      ADD_FAILURE() << "Unable to parse " << exip_xml;
      if (auto S = exip_doc.data(); !S.empty())
        fmt::println("Data: {}", S);
      CONTINUE_FAIL();
    }

    //////////////////////////////////////////////////////////////////////
    // Compare Files

    bool cmp = compare_xml(
      exip_doc.document(), exif_doc.get(), cmp_opts);
    if (!cmp) {
      // No indent
      std::cout << "exip: ";
      rapidxml::print(std::cout, *exip_doc.document(), 1);
      std::cout << "\nexif: ";
      rapidxml::print(std::cout, *exif_doc, 1);
      std::cout << std::endl;
      ADD_FAILURE() << "EXI outputs were not equivalent: " << (*file);
      CONTINUE_FAIL();
    }
  }

  constexpr std::size_t no_errors = 0;
  ASSERT_EQ(failure_count, no_errors) << "Error decoding files"
    << (failure_count > error_max
      ? ", quit early after too many errors."
      : "."
    );
}

//////////////////////////////////////////////////////////////////////////
// Instance

#if EXICPP_FULL_TESTS
using TestTuple = OptsTuple;
# define INJECT_OPTS() Bool(), Bool(), Bool(),
# define INJECT_PRES() Bool(), Bool(),
#else
using TestTuple = OptsTupleSlim;
# define INJECT_OPTS()
# define INJECT_PRES()
#endif

using ::testing::Bool;
using TestType = testing::TestParamInfo<CompareExi::ParamType>;

auto getName(const TestType& I) -> std::string {
  const Opts& O = I.param;
  return O.getName();
};

constexpr std::array AlignVals {
  AL_BitPacked, AL_BytePacked,
  AL_PreCompression, AL_Compression
};

INSTANTIATE_TEST_SUITE_P(Conformance, CompareExi,
  testing::ConvertGenerator<TestTuple>(
    testing::Combine(
      // Strict, Fragment, SelfContained
      INJECT_OPTS()
      testing::ValuesIn(AlignVals),
      // Comments
      Bool(),
      // Preserve[PIs, DTDs]
      INJECT_PRES()
      // Prefixes, LexVals
      Bool(), Bool()
    )
  ),
  &getName
);

} // namespace `anonymous`
