//===- Driver.cpp ---------------------------------------------------===//
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

#include <exicpp/Reader.hpp>
#include <exicpp/Writer.hpp>
#include <exicpp/XML.hpp>
#include "Driver.hpp"

#define RAPIDXML_NO_STREAMS
#include <algorithm>
#include <fstream>
#include <iterator>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <rapidxml_print.hpp>
#include <fmt/color.h>

#define COLOR_PRINT_(col, fstr, ...) \
  fmt::print(fstr, fmt::styled( \
    fmt::format(__VA_ARGS__), fmt::fg(col)))
#define COLOR_PRINT(col, ...) COLOR_PRINT_(col, "{}", __VA_ARGS__)
#define COLOR_PRINTLN(col, ...) COLOR_PRINT_(col, "{}\n", __VA_ARGS__)

using namespace exi;

using Str = std::basic_string<Char>;

template <typename T>
using Option = std::optional<T>;

template <typename K, typename V>
using Map = std::unordered_map<K, V>;

template <typename K>
using Set = std::unordered_set<K>;

//////////////////////////////////////////////////////////////////////////

struct InternRef : public StrRef {
  using BaseType = StrRef;
public:
  constexpr InternRef() noexcept = default;
  InternRef(Char* data, std::size_t len) noexcept : BaseType(data, len) { }
  constexpr InternRef(const InternRef&) noexcept = default;
  constexpr InternRef& operator=(const InternRef&) noexcept = default;
public:
  Char* data() const { return const_cast<Char*>(BaseType::data()); }
};

struct XMLBuilder {
  using Ty = XMLType;

  XMLBuilder() :
    doc(std::make_unique<XMLDocument>()),
    node(doc->document()) {
  }

  static StrRef GetXMLHead() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
  }

  void dump() {
    // std::cout << GetXMLHead() << '\n';
    // std::cout << *doc << std::endl;
  }

  void dump(const fs::path& outpath) {
    std::ofstream os(outpath, std::ios::binary);
    os.unsetf(std::ios::skipws);

    if (!os) {
      COLOR_PRINTLN(fmt::color::red,
        "ERROR: Unable to write to file '{}'", outpath);
      return;
    }

    os << GetXMLHead() << '\n';
    rapidxml::print(
      std::ostream_iterator<Char>(os), *this->doc);
  }

public:
  ErrCode startDocument() {
    this->node = doc->document();
    LOG_ASSERT(node && node->type() == Ty::node_document);
    return ErrCode::Ok;
  }

  ErrCode endDocument() {
    LOG_ASSERT(node && node->type() == Ty::node_document);
    return ErrCode::Ok;
  }

  ErrCode startElement(const QName& name) {
    const auto ln = this->internQName(name);
    auto ty = ln.empty() ? Ty::node_data : Ty::node_element;
    auto* newNode = this->makeNode<true>(ty, ln);
    // Add as child
    node->append_node(newNode);
    this->node = newNode;
    return ErrCode::Ok;
  }

  ErrCode endElement() {
    LOG_ASSERT(node);
    this->node = node->parent();
    return ErrCode::Ok;
  }

  ErrCode namespaceDeclaration(
    StrRef ns,
    StrRef prefix,
    bool isLocal) 
  {
    if (isLocal && !prefix.empty()) {
      StrRef name(node->name(), node->name_size());
      auto fullName = fmt::format("{}:{}", prefix, name);
      InternRef iname = this->intern(fullName);
      node->name(iname.data(), iname.size());
    }

    auto fullPre = XMLBuilder::FormatNs(prefix);
    auto* attr = this->makeAttribute(fullPre, ns);
    node->append_attribute(attr);
    return ErrCode::Ok;
  }

  ErrCode attribute(const QName& name) {
    LOG_ASSERT(!this->attr);
    this->attr = this->makeAttribute(name.localName());
    node->append_attribute(attr);
    return ErrCode::Ok;
  }

  ErrCode stringData(StrRef str) {
    if (this->attr) {
      InternRef istr = this->intern(str);
      attr->value(istr.data(), istr.size());
      this->attr = nullptr;
      return ErrCode::Ok;
    }

    LOG_ASSERT(node->type() == Ty::node_data);
    InternRef istr = this->intern(str);
    node->value(istr.data(), istr.size());
    return ErrCode::Ok;
  }

private:
  static std::string FormatNs(StrRef prefix) {
    if (prefix.empty())
      return "xmlns";
    return fmt::format("xmlns:{}", prefix);
  }

  InternRef internQName(const QName& qname) {
    const auto prefix = qname.prefix();
    if (prefix.empty()) {
      return this->intern(qname.localName());
    }
    
    auto fullName = fmt::format("{}:{}", prefix, qname.localName());
    return this->intern(fullName);
  }

  template <bool AssureInterned = false>
  InternRef intern(StrRef str) {
    if (str.empty())
      return {nullptr, 0};
    if constexpr (AssureInterned) {
      auto* raw = const_cast<Char*>(str.data());
      return {raw, str.size()};
    }
    auto it = intern_table.find(str);
    if (it != intern_table.end())
      return it->second;
    return this->makePooledStr(str);
  }

  InternRef makePooledStr(StrRef str) {
    if (str.empty())
      return {nullptr, 0};
    const std::size_t len = str.size();
    Char* rawStr = doc->allocate_string(nullptr, len);
    std::char_traits<Char>::copy(rawStr, str.data(), len);

    InternRef is {rawStr, len};
    LOG_ASSERT(intern_table.count(str) == 0);
    intern_table[str] = is;
    return is;
  }

  template <bool AssureInterned = false>
  XMLNode* makeNode(Ty type, StrRef name = "", StrRef value = "") {
    auto iname = this->intern<AssureInterned>(name);
    auto ivalue = this->intern<AssureInterned>(value);
    return doc->allocate_node(type,
      iname.data(), ivalue.data(),
      iname.size(), ivalue.size()
    );
  }

  template <bool AssureInterned = false>
  XMLAttribute* makeAttribute(StrRef name, StrRef value = "") {
    auto iname = this->intern<AssureInterned>(name);
    auto ivalue = this->intern<AssureInterned>(value);
    return doc->allocate_attribute(
      iname.data(), ivalue.data(),
      iname.size(), ivalue.size()
    );
  }

private:
  Box<XMLDocument> doc;
  XMLNode* node = nullptr;
  XMLAttribute* attr = nullptr;
  Map<StrRef, InternRef> intern_table;
};

//////////////////////////////////////////////////////////////////////////

Mode progMode = Mode::Default;

Option<fs::path> inpath;
Option<fs::path> outpath;

bool includeOptions = true;
bool includeCookie = true;
bool preservePrefixes = true;

static void printHelp();
static void encodeXML();
static void decodeEXI();
static void encodeDecode();

static Str normalizeCommand(StrRef S) {
  auto lower = +[](char c) -> char { return std::tolower(c); };
  std::string outstr(S.size(), '\0');
  std::transform(S.begin(), S.end(), outstr.begin(), lower);
  return outstr;
}

static fs::path validatePath(StrRef path) {
  fs::path outpath(path);
  if (fs::exists(outpath))
    return outpath;
  COLOR_PRINTLN(fmt::color::red,
    "ERROR: Invalid path '{}'", path);
  std::exit(1);
}

static void processCommand(ArgProcessor& P) {
  auto cmd = P.curr();
  cmd.remove_prefix(1);
  auto str = normalizeCommand(cmd);

  if (str == "h" || str == "help") {
    printHelp();
    std::exit(0);
  }
  
  if (str == "i" || str == "-input") {
    fs::path path = validatePath(P.peek());
    inpath.emplace(path);
    P.next();
  } else if (str == "o" || str == "-output") {
    fs::path path = fs::absolute(P.peek());
    outpath.emplace(path);
    P.next();
  } else if (str == "e" || str == "-encode") {
    progMode = Mode::Encode;
  } else if (str == "d" || str == "-decode") {
    progMode = Mode::Decode;
  } else if (str == "ed" || str == "-encodedecode") {
    progMode = Mode::EncodeDecode;
  } else if (str == "includeoptions") {
    includeOptions = true;
  } else if (str == "includecookie") {
    includeCookie = true;
  } else if (str == "preserveprefixes") {
    preservePrefixes = true;
  }
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printHelp();
    return 0;
  }

  ArgProcessor P {argc, argv};
  while (P) {
    auto str = P.curr();
    if (str.empty()) {
      P.next();
      continue;
    }

    if (str.front() != '-') {
      COLOR_PRINTLN(fmt::color::red,
        "ERROR: Unknown option '{}'", str);
      return 1;
    }

    processCommand(P);
    P.next();
  }

  if (progMode == Mode::Help) {
    printHelp();
    return 0;
  }

  if (!inpath) {
    COLOR_PRINTLN(fmt::color::red,
      "ERROR: 'inpath' must be specified in this mode.");
    return 1;
  }

  switch (progMode) {
   case Encode:
    encodeXML();
    break;
   case Decode:
    decodeEXI();
    break;
   case EncodeDecode:
    encodeDecode();
    break;
  }
}

using BufferType = InlineStackBuffer<512>;

void printHelp() {
  fmt::println(
    "\nCOMMAND LINE OPTIONS:\n"
    "  -h, --help:            Prints help\n"
    "  -e, --encode:          Encode XML as EXI\n"
    "  -d, --decode:          Decode EXI as XML\n"
    "  -ed, --enodeDecode:    Decode EXI as XML\n"
    "  \n"
    "  -i, --input  <file>:   Input file\n"
    "  -o, --output <file>:   Output file (optional)\n"
    "  \n"
    "  -includeOptions\n"
    "  -includeCookie\n"
    "  -preservePrefixes\n"
  );
}

static fs::path outpathOr(fs::path inpath, const Str& ext) {
  return outpath.value_or(
    inpath.replace_extension(ext));
}

void encodeXML() {
  fs::path xml = fs::absolute(*inpath);
  fs::path exi = outpathOr(xml, "exi");
  fmt::println("Reading from '{}'", xml);
  
  auto xmldoc = BoundDocument::ParseFrom(xml);
  if (!xmldoc) {
#if !EXICPP_DEBUG
    COLOR_PRINTLN(fmt::color::red,
      "Unable to locate file '{}'!", xml);
#endif
    std::exit(1);
  }

  BufferType buf {};
  auto existr = to_multibyte(exi);
  if (Error E = buf.writeFile(existr)) {
    COLOR_PRINTLN(fmt::color::red,
      "Error in '{}': {}", exi, E.message());
    std::exit(1);
  }

  if (Error E = write_xml(xmldoc.document(), buf)) {
    COLOR_PRINTLN(fmt::color::red,
      "Error in '{}': {}", exi, E.message());
    std::exit(1);
  }

  COLOR_PRINTLN(fmt::color::light_green,
    "Wrote to '{}'", exi);
}

void decodeEXI() {
  fs::path exi = fs::absolute(*inpath);
  fs::path xml = outpathOr(exi, "xml");;
  fmt::println("Reading from '{}'", exi);

  BufferType buf {};
  auto existr = to_multibyte(exi);
  if (Error E = buf.readFile(existr)) {
    COLOR_PRINTLN(fmt::color::red,
      "Error in '{}': {}", exi, E.message());
    std::exit(1);
  }

  XMLBuilder builder {};
  auto parser = Parser::New(builder, buf);

  if (Error E = parser.parseHeader()) {
    COLOR_PRINTLN(fmt::color::red,
      "\nError in '{}'\n", exi);
    std::exit(1);
  }

  if (Error E = parser.parseAll()) {
    COLOR_PRINTLN(fmt::color::red,
      "\nError in '{}'\n", exi);
    std::exit(1);
  }

  builder.dump(xml);
  COLOR_PRINTLN(fmt::color::light_green,
    "Wrote to '{}'", xml);
}

void encodeDecode() {
  fs::path xml = fs::absolute(*inpath);
  fs::path exi = outpathOr(xml, "exi");

  COLOR_PRINTLN(fmt::color::yellow,
    "-encodeDecode is currently unimplemented!");
  std::exit(0);
}
