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

#undef RAPIDXML_NO_EXCEPTIONS
#include <Common/SmallStr.hpp>
#include <Support/Filesystem.hpp>
#include <Support/Logging.hpp>
#include <Support/MemoryBuffer.hpp>
#include <Support/MemoryBufferRef.hpp>
#include <Support/Process.hpp>
#include <Support/ScopedSave.hpp>
#include <Support/raw_ostream.hpp>
#include <rapidxml.hpp>

#define DEBUG_TYPE "__DRIVER__"

using namespace exi;

using XMLDocument  = xml::XMLDocument<Char>;
using XMLAttribute = xml::XMLAttribute<Char>;
using XMLBase      = xml::XMLBase<Char>;
using XMLNode      = xml::XMLNode<Char>;
using XMLType      = xml::NodeKind;

namespace {
class XMLErrorInfo : public ErrorInfo<XMLErrorInfo> {
  friend class ErrorInfo<XMLErrorInfo>;
  static char ID;

  String Msg;
  usize Offset = 0;
public:
  XMLErrorInfo(const Twine& Msg,
               usize Offset = usize(-1)) :
   Msg(Msg.str()), Offset(Offset) {}

  void log(raw_ostream& OS) const override {
    OS << "XML Error";
    if (Offset != usize(-1))
      OS << " at " << Offset;
    if (!Msg.empty())
      OS << ": " << Msg;
  }

  std::error_code convertToErrorCode() const override {
    return std::error_code(); // ?
  }
};

char XMLErrorInfo::ID = 0;
} // namespace `anonymous`

static Expected<Box<XMLDocument>>
 ParseXMLFromMemoryBuffer(WritableMemoryBuffer& MB) {
  ScopedSave S(xml::use_exceptions_anyway, true);
  outs() << "Reading file \'" << MB.getBufferIdentifier() << "\'\n";
  try {
    static constexpr int ParseRules
      = xml::parse_declaration_node | xml::parse_all;
    auto Doc = std::make_unique<XMLDocument>();
    exi_assert(Doc.get() != nullptr);
    Doc->parse<ParseRules>(MB.getBufferStart());
    return std::move(Doc);
  } catch (const std::exception& Ex) {
#ifndef RAPIDXML_NO_EXCEPTIONS
    LOG_ERROR("Failed to read file '{}'", MB.getBufferIdentifier());
    /// Check if it's rapidxml's wee type
    if (auto* PEx = dynamic_cast<const xml::parse_error*>(&Ex)) {
      LOG_EXTRA("Error type is 'xml::parse_error'");
      const usize Off = MB.getBufferOffset(PEx->where<Char>());
      return make_error<XMLErrorInfo>(PEx->what(), Off);
    }
#endif
    return make_error<XMLErrorInfo>(Ex.what());
  }
}

int tests_main(int Argc, char* Argv[]);
int main(int Argc, char* Argv[]) {
  exi::DebugFlag = LogLevel::WARN;
  outs().enable_colors(true);
  dbgs().enable_colors(true);

  ExitOnError ExitOnErr("exicpp: ");
  SmallStr<80> Path("examples/Namespace.xml");
  sys::fs::make_absolute(Path);

  Box<WritableMemoryBuffer> MB = ExitOnErr(
    errorOrToExpected(WritableMemoryBuffer::getFile(Path)));
  Box<XMLDocument> Doc = ExitOnErr(
    ParseXMLFromMemoryBuffer(*MB));
  outs() << raw_ostream::BRIGHT_GREEN
    << "Read success!\n" << raw_ostream::RESET;

  // tests_main(Argc, Argv);
}

#ifdef RAPIDXML_NO_EXCEPTIONS
void xml::parse_error_handler(const char* what, void* where) {
  if (xml::use_exceptions_anyway)
    throw xml::parse_error(what, where);
  errs() << "Uhhhh... " << what << '\n';
  sys::Process::Exit(1);
}
#endif // RAPIDXML_NO_EXCEPTIONS

bool xml::use_exceptions_anyway = false;
