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

// #define RAPIDXML_NO_STREAMS
#include <iostream>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <rapidxml_print.hpp>

#include "CompareXML.hpp"
#include "Print.hpp"
#include "STL.hpp"

using namespace exi;

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

struct FilestreamBuf {
  using StreamType = std::basic_ofstream<Char>;

  struct iterator {
    iterator(FilestreamBuf& buf) : pbuf(&buf) {}
    ALWAYS_INLINE Char& operator*() { return curr; }
    iterator& operator++() {
      pbuf->pushChar(curr);
      return *this;
    }
    iterator operator++(int) {
      iterator cpy = *this;
      ++*this;
      return cpy;
    }
  private:
    FilestreamBuf* pbuf;
    Char curr = Char('\0');
  };

public:
  FilestreamBuf(StreamType& os, std::size_t n) : buffer(), os(os) {
    buffer.reserve(n);
  }

  ~FilestreamBuf() {
    this->flush();
  }

public:
  inline void pushChar(Char c) {
    buffer.push_back(c);
    if EXICPP_UNLIKELY(this->atCapacity())
      this->flush();
  }

  inline iterator getIter() {
    return iterator{*this};
  }

private:
  bool atCapacity() const {
    return (buffer.size() == buffer.capacity());
  }

  void flush() {
    os.write(buffer.data(), buffer.size());
    buffer.resize(0);
  }

private:
  Vec<Char> buffer;
  StreamType& os;
};

#define NOINTERN 1

struct XMLBuilder {
  using Ty = XMLType;

  XMLBuilder() :
   doc(std::make_unique<XMLDocument>()),
   node(doc->document()) {
    set_xml_allocators(doc.get());
  }

  static StrRef GetXMLHead() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
  }

  XMLDocument* document() { return doc.get(); }
  const XMLDocument* document() const { return doc.get(); }

  void dump() {
    std::cout << GetXMLHead() << '\n';
    std::cout << *doc << std::endl;
  }

  void dump(const fs::path& outpath) {
    std::basic_ofstream<Char> os(outpath, std::ios::binary);
    os.unsetf(std::ios::skipws);

    if (!os) {
      COLOR_PRINTLN(fmt::color::red,
        "ERROR: Unable to write to file '{}'", outpath);
      return;
    }

    os << GetXMLHead() << '\n';
    // TODO: fix?
    // FilestreamBuf fstr(os, 2048);
    // rapidxml::print(fstr.getIter(), *this->doc);
    rapidxml::print(std::ostream_iterator<Char>(os), *this->doc);
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

    // LOG_ASSERT(node->type() == Ty::node_data);
    if EXICPP_UNLIKELY(node->type() != Ty::node_data) {
#if EXICPP_DEBUG
      LOG_WARN("Expected 'node_data', got '{}'",
        unsigned(node->type()));
      SetVerbose(true);
#endif
      return ErrCode::Ok;
    }
    InternRef istr = this->intern(str);
    node->value(istr.data(), istr.size());
    return ErrCode::Ok;
  }

private:
  static void SetVerbose(bool V);
  static Str ReplaceNonprintable(StrRef S);

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
#if NOINTERN
    return this->makePooledStr(str);
#else
    auto it = intern_table.find(str);
    if (it != intern_table.end())
      return it->second;
    return this->makePooledStr(str);
#endif
  }

  InternRef makePooledStr(StrRef str) {
    if EXICPP_UNLIKELY(str.empty())
      return {nullptr, 0};
    const std::size_t len = str.size();
    Char* rawStr = doc->allocate_string(nullptr, len);
    std::char_traits<Char>::copy(rawStr, str.data(), len);

    InternRef is {rawStr, len};
#if !NOINTERN
    LOG_ASSERT(intern_table.count(str) == 0);
    intern_table[str] = is;
#endif
    return is;
  }

  template <bool AssureInterned = false>
  XMLNode* makeNode(Ty type, StrRef name = "", StrRef value = "") {
    auto iname = this->intern<AssureInterned>(name);
    auto ivalue = this->intern<AssureInterned>(
      XMLBuilder::ReplaceNonprintable(value));
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
#if !NOINTERN
  Map<StrRef, InternRef> intern_table;
#endif
};

//////////////////////////////////////////////////////////////////////////

static Mode progMode = Mode::Default;
static bool verbose = false;
static bool doDump = false;
static bool replaceNonprintable = false;

static Option<fs::path> inpath;
static Option<fs::path> outpath;

static bool comparexml = false;
static bool includeCookie = false;
// ...
static bool preserveComments = false;
static bool preservePIs = false;
static bool preserveDTs = false;
static bool preservePrefixes = true;

static Option<Options> opts = std::nullopt;

static void printHelp();
static void encodeXML(bool doPrint = true);
static void decodeEXI(bool doPrint = true);
static void encodeDecode(bool doPrint = true);

static Str normalizeCommand(StrRef S) {
  auto lower = +[](char c) -> char { return std::tolower(c); };
  Str outstr(S.size(), '\0');
  std::transform(S.begin(), S.end(), outstr.begin(), lower);
  return outstr;
}

static fs::path validatePath(StrRef path) {
  fs::path outpath(path);
  if (fs::exists(outpath))
    return outpath;
  PRINT_ERR("Invalid path '{}'.", path);
  std::exit(1);
}

static bool setPath(Option<fs::path>& toSet, const fs::path& path) {
  if (toSet.has_value())
    return false;
  toSet.emplace(path);
  return true;
}

[[maybe_unused]] Str XMLBuilder::ReplaceNonprintable(StrRef S) {
  if (!replaceNonprintable)
    return Str{S.data(), S.size()};

  std::size_t I = 0, window = 0;
  const std::size_t E = S.size();

  fmt::basic_memory_buffer<Char, 64> out;
  out.reserve(S.size());
  auto push = [&, S] (bool extra = false) {
    out.append(S.substr(I, window - (I + extra)));
    I = window;
  };

  while (window < E) {
    using UChar = std::make_unsigned_t<Char>;
    const auto C = UChar(S[window++]);
    if (C >= UChar(' '))
      continue;
    push(true);
    fmt::format_to(std::back_inserter(out), "&#{};", unsigned(C));
  }

  push();
  return Str{out.data(), out.size()};
}

void XMLBuilder::SetVerbose(bool V) {
  verbose = V;
}

static void checkVerbose(ArgProcessor& P) {
  for (StrRef cmd : P) {
    const auto str = normalizeCommand(cmd);
    if (str == "-v" || str == "--verbose") {
      PRINT_INFO("Enabled verbose output.");
#if !EXICPP_DEBUG
      PRINT_WARN("Debug printing has been disabled.");
#endif
      verbose = true;
      break;
    }
  }

  if (!verbose)
    return;

  fmt::print("Command line:");
  for (StrRef cmd : P) {
    const auto len = cmd.size();
    fmt::print(" {}", cmd);
  }
  fmt::println("");
}

static void initOptions() {
  if EXICPP_LIKELY(opts) return;
  opts.emplace();
}

static void setPreserved(ArgProcessor& P, StrRef cmd) {
  LOG_ASSERT(cmd.empty() == false);
  initOptions();

  if (cmd == "preserveprefixes") {
    // Legacy support.
    opts->set(Preserve::Prefixes);
    preservePrefixes = true;
    return;
  }

  cmd.remove_prefix(1);
  if (cmd.empty()) {
    LOG_INFO("Preserving all values.");
    opts->set(Preserve::All);
    return;
  } else if (cmd.size() > 1) {
    PRINT_WARN("Unknown command '{}', ignoring.", P.curr());
    return;
  }

  switch (cmd.front()) {
  case 'c':
    opts->set(Preserve::Comments);
    break;
  case 'i':
    opts->set(Preserve::PIs);
    break;
  case 'd':
    opts->set(Preserve::DTD);
    break;
  case 'p':
    opts->set(Preserve::Prefixes);
    break;
  case 'l':
    opts->set(Preserve::LexicalValues);
    break;
  case 'a':
    opts->set(Preserve::All);
    break;
  default:
    PRINT_WARN("Unknown command '{}', ignoring.", P.curr());
  };
}

static void setEnumOpt(ArgProcessor& P, StrRef cmd) {
  LOG_ASSERT(cmd.empty() == false);
  initOptions();

  auto warnUnknown = [&P] () {
    PRINT_WARN("Unknown command '{}', ignoring.", P.curr());
  };

  cmd.remove_prefix(1);
  if (cmd.empty()) {
    warnUnknown();
    return;
  }

  switch (cmd.front()) {
  case 'c':
    opts->set(EnumOpt::Compression);
    break;
  case 'f':
    opts->set(EnumOpt::Fragment);
    break;
  case 's': {
    if (cmd.size() == 2) {
      if (cmd[1] == 't')
        opts->set(EnumOpt::Strict);
      else if (cmd[1] == 'c')
        opts->set(EnumOpt::SelfContained);
      else
        warnUnknown();
      return;
    }
    // Long names...
    if (cmd == "strict")
      opts->set(EnumOpt::Strict);
    else if (cmd == "self" || cmd == "selfcontained")
      opts->set(EnumOpt::SelfContained);
    else
      warnUnknown();
    break;
  }
  case 'a': {
    cmd.remove_prefix(1);
    if (cmd.empty()) {
      warnUnknown();
      return;
    }
    // Compression types
    if (cmd.front() == 'b') {
      if (cmd == "bit" || cmd == "bitpacked")
        opts->set(Align::BitPacked);
      else if (cmd == "byte" || cmd == "bytealigned")
        opts->set(Align::ByteAlignment);
      else
        warnUnknown();
      return;
    } else if (cmd.front() == 'p') {
      if (cmd == "packed")
        opts->set(Align::BitPacked);
      else if (cmd == "pre" || cmd == "precompression")
        opts->set(Align::PreCompression);
      else
        warnUnknown();
      return;
    }
    [[fallthrough]];
  }
  default:
    warnUnknown();
  };
}

static void setAlignOpt(ArgProcessor& P, StrRef cmd) {
  LOG_ASSERT(cmd.empty() == false);
  initOptions();

  auto warnUnknown = [&P] () {
    PRINT_WARN("Unknown command '{}', ignoring.", P.curr());
  };

  cmd.remove_prefix(1);
  if (cmd.empty()) {
    warnUnknown();
    return;
  }

  if (cmd == "bit" || cmd == "bitPacked") {
    opts->set(Align::BitPacked);
  } else if (cmd == "byte" || cmd == "bytePacked") {
    opts->set(Align::BytePacked);
  } else if (cmd == "pre" || cmd == "preCompression") {
    opts->set(Align::PreCompression);
  } else if (cmd == "com" || cmd == "compression") {
    opts->set(EnumOpt::Compression);
  } else {
    warnUnknown();
  }
}

static void processCommand(ArgProcessor& P) {
  auto cmd = P.curr();
  cmd.remove_prefix(1);
  const auto str = normalizeCommand(cmd);

  if (str.empty()) {
    PRINT_WARN("Empty command!");
    return;
  }

  if (str == "h" || str == "help") {
    printHelp();
    std::exit(0);
  } else if (str == "v" || str == "-verbose") {
    return;
  }

  if (str.front() == 'p') {
    setPreserved(P, str);
    return;
  } else if (str.front() == 'o' && str.size() > 1) {
    setEnumOpt(P, str);
    return;
  } else if (str.front() == 'a') {
    setAlignOpt(P, str);
    return;
  }
  
  if (str == "i" || str == "-input") {
    fs::path path = validatePath(P.peek());
    if (!setPath(inpath, path))
      PRINT_WARN("Input path has already been set.");
    P.next();
  } else if (str == "o" || str == "-output") {
    fs::path path = fs::absolute(P.peek());
    if (!setPath(outpath, path))
      PRINT_WARN("Output path has already been set.");
    P.next();
  } else if (str == "-dump") {
    PRINT_INFO("Dumping for decode.");
    doDump = true;
  } else if (str == "e" || str == "-encode") {
    progMode = Mode::Encode;
  } else if (str == "d" || str == "-decode") {
    progMode = Mode::Decode;
  } else if (str == "ed" || str == "-encodedecode") {
    progMode = Mode::EncodeDecode;
  } else if (str == "comparexml") {
    comparexml = true;
  } else if (str == "replacecontrol") {
    replaceNonprintable = true;
  } else if (str == "includecookie" || str == "cookie") {
    includeCookie = true;
  } else if (str == "includeoptions") {
    initOptions();
  } else {
    PRINT_WARN("Unknown command '{}', ignoring.", P.curr());
  }
}

static int driverMain(int argc, char* argv[]) {
  if (argc < 2) {
    printHelp();
    return 0;
  }

  ArgProcessor P {argc, argv};
  checkVerbose(P);
  while (P) {
    auto str = P.curr();
    if (str.empty()) {
      P.next();
      continue;
    }

    if (str.front() != '-') {
      PRINT_WARN("Unknown input '{}', ignoring.", str);
      P.next();
      continue;
    }

    processCommand(P);
    P.next();
  }

  if (progMode == Mode::Help) {
    printHelp();
    return 0;
  }

  if (!inpath) {
    PRINT_ERR("Input path must be specified with '-i' in this mode.");
    return 1;
  }

  if (opts.has_value()) {
    preserveComments = opts->isSet(Preserve::Comments);
    preservePrefixes = opts->isSet(Preserve::Prefixes);
    preserveDTs = opts->isSet(Preserve::DTD);
    preservePIs = opts->isSet(Preserve::PIs);
  }

  if (verbose)
    fmt::println("");

  DEBUG_SET_MODE(verbose);
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

  return 0;
}

int main(int argc, char* argv[]) {
  try {
    return driverMain(argc, argv);
  } catch (const std::exception& e) {
    fmt::println("");
    PRINT_ERR("Exception thrown: {}", e.what());
  }
}

using BufferType = InlineStackBuffer<4096>;

void printHelp() {
  fmt::println(
    "\nCOMMAND LINE OPTIONS:\n"
    " MODE:\n"
    "  -h,  --help:           Prints help\n"
    "  -v,  --verbose:        Prints extra information (if available)\n"
    "  -e,  --encode:         Encode XML as EXI\n"
    "  -d,  --decode:         Decode EXI as XML\n"
    "  -ed, --enodeDecode:    Decode EXI as XML\n"
    "\n IO:\n"
    "  -i, --input  <file>:   Input file\n"
    "  -o, --output <file>:   Output file (optional)\n"
    "  \n"
    "\n EXI SPECIFIC:\n"
    "  -includeOptions\n"
    "  -includeCookie\n"
    "  -preservePrefixes\n"
    "  \n"
    // TODO: Add these
    "  -A\n"
    "  -O\n"
    "  -P\n"
    "  \n"
    "\n MISC:\n"
    "  -compareXML:           Check if output XML instead of writing out\n"
    "  -replaceControl:       Replace control characters with XML escapes\n"
  );
}

static fs::path outpathOr(fs::path reppath, const Str& ext) {
  if (outpath.has_value())
    return fs::absolute(*outpath);
  reppath = reppath.replace_extension(ext);
  if (verbose)
    PRINT_INFO("Output path not specified, set to '{}'", reppath);
  return reppath;
}

void encodeXML(bool doPrint) {
  fs::path xmlIn = fs::absolute(*inpath);
  fs::path exi = outpathOr(xmlIn, "exi");
  fmt::println("Reading from '{}'", xmlIn);
  
  auto xmldoc = BoundDocument::ParseFrom(xmlIn);
  if (!xmldoc) {
#if !EXICPP_DEBUG
    PRINT_ERR("Error encoding '{}'!", xmlIn);
#endif
    std::exit(1);
  }

  // BufferType buf {};
  HeapBuffer bufBase {(2048 * 32) - 1};
  BinaryBuffer buf {bufBase};
  if (Error E = buf.writeFile(exi)) {
    COLOR_PRINTLN(fmt::color::red,
      "Error opening '{}': {}", exi, E.message());
    std::exit(1);
  }

  fmt::println("Writing to '{}'", exi);
  if (Error E = write_xml(xmldoc.document(), buf, opts)) {
    COLOR_PRINTLN(fmt::color::red,
      "Error with '{}': {}", xmlIn, E.message());
    std::exit(1);
  }

  if (doPrint) {
    COLOR_PRINTLN(fmt::color::light_green,
      "Wrote to '{}'", exi);
  }
}

void decodeEXI(bool doPrint) {
  fs::path exiIn = fs::absolute(*inpath);
  fs::path xml = outpathOr(exiIn, "xml");
  fmt::println("Reading from '{}'", exiIn);

  BufferType buf {};
  if (Error E = buf.readFile(exiIn)) {
    COLOR_PRINTLN(fmt::color::red,
      "Error opening '{}': {}", exiIn, E.message());
    std::exit(1);
  }

  XMLBuilder builder {};
  auto parser = Parser::New(builder, buf);

  if (Error E = parser.parseHeader()) {
    COLOR_PRINTLN(fmt::color::red,
      "\nError in '{}': {}\n",
      exiIn, E.message());
    std::exit(1);
  }

  fmt::println("Parsing to XML...");
  if (Error E = parser.parseAll()) {
    COLOR_PRINTLN(fmt::color::red,
      "\nError in '{}': {}\n", exiIn, E.message());
    std::exit(1);
  }

  if (doDump)
    builder.dump();
  else
    builder.dump(xml);

  if (!doDump && doPrint) {
    COLOR_PRINTLN(fmt::color::light_green,
      "Wrote to '{}'", xml);
  }
}

//////////////////////////////////////////////////////////////////////////

static fs::path addExtension(fs::path path, const Str& ext) {
  if (!path.has_extension()) {
    return path.replace_extension(ext);
  }
  return path.replace_extension(
    fmt::format("{}.{}", path.extension(), ext));
}

static bool compareXML(XMLDocument* oldDoc, XMLDocument* newDoc) {
  CompareOpts O {
    preserveComments,
    preservePIs, preserveDTs,
    verbose
  };
  return compare_xml(oldDoc, newDoc, O);
}

void encodeDecode(bool doPrint) {
  fs::path xmlIn = fs::absolute(*inpath);
  fs::path exi = addExtension(xmlIn, "exi");
  Option<fs::path> xmlOut = outpath;

  if (!xmlOut) {
    // We will properly initialize xmlOut, even if it is not used.
    xmlOut.emplace(addExtension(exi, "xml"));
    if (!comparexml && verbose)
      PRINT_INFO("Output path not specified, set to '{}'", *xmlOut);
  }

  // Set the global output path.
  outpath.emplace(exi);
  encodeXML(false);

  fmt::println("Reading from intermediate file '{}'", exi);
  // BufferType buf {};
  HeapBuffer bufBase {(2048 * 32) - 1};
  BinaryBuffer buf {bufBase};
  if (Error E = buf.readFile(exi)) {
    COLOR_PRINTLN(fmt::color::red,
      "Error opening '{}': {}", exi, E.message());
    std::exit(1);
  }

  XMLBuilder builder {};
  auto parser = Parser::New(builder, buf);

  if (Error E = parser.parseHeader()) {
    COLOR_PRINTLN(fmt::color::red,
      "\nError parsing header in '{}'\n", exi);
    std::exit(1);
  }

  if (Error E = parser.parseAll()) {
    COLOR_PRINTLN(fmt::color::red,
      "\nError in '{}'\n", exi);
    std::exit(1);
  }

  if (comparexml) {
    // Load the original document and compare it to the new one.
    auto xmldoc = BoundDocument::ParseFrom<rapidxml::parse_no_element_values>(xmlIn);
    fmt::println("Comparing XML...");
    const bool res = compareXML(xmldoc.document(), builder.document());
    if (res) {
      COLOR_PRINTLN(fmt::color::light_green,
        "Input XML was equivalent to output!");
    }
    return;
  }

  if (doDump)
    // Dump to output stream.
    builder.dump();
  else
    builder.dump(*xmlOut);
  
  if (!doDump && doPrint) {
    COLOR_PRINTLN(fmt::color::light_green,
      "Wrote to '{}'", *xmlOut);
  }
}
