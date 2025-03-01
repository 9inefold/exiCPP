//===- XMLDumper.cpp ------------------------------------------------===//
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

#include "Driver.hpp"
#include <Common/SmallStr.hpp>
#include <Common/IntrusiveRefCntPtr.hpp>
#include <Common/MMatch.hpp>
#include <Support/Format.hpp>
#include <Support/MemoryBuffer.hpp>
#include <Support/ScopedSave.hpp>
#include <Support/raw_ostream.hpp>
#include <exi/Basic/XMLManager.hpp>
#include <exi/Basic/XMLContainer.hpp>
#include <algorithm>
#include <rapidxml.hpp>

using namespace exi;

namespace {
class XMLDumper {
  static constexpr StrRef knode_unknown = "UNKNOWN-TYPE"; 
  static constexpr StrRef knode_null = "NULL-TYPE"; 

  XMLDocument& TopLevel;
  exi::indent Indent;
  raw_ostream& OS;

public:
  using enum raw_ostream::Colors;
  using Colors = raw_ostream::Colors;

  bool DebugPrint = false;

  Colors COLOR_default = CYAN;
  Colors COLOR_name    = BRIGHT_CYAN;
  Colors COLOR_dtname  = BRIGHT_YELLOW;
  Colors COLOR_ns      = BLUE;
  Colors COLOR_attr    = BRIGHT_MAGENTA;
  Colors COLOR_attrns  = BRIGHT_BLUE;
  Colors COLOR_string  = BRIGHT_GREEN;
  Colors COLOR_cdata   = BRIGHT_GREEN;
  Colors COLOR_split   = BLACK;
  Colors COLOR_comment = BRIGHT_BLACK;
  Colors COLOR_entity  = BRIGHT_RED;
  Colors COLOR_data    = BRIGHT_WHITE;

public:
  using NodeT = const XMLNode*;
  XMLDumper(XMLDocument& Doc, int IndentLevel = 2,
            Option<raw_ostream&> OS = std::nullopt) :
   TopLevel(Doc), OS(OS.value_or(nulls())),
   Indent(0, IndentLevel) {
  }

  /// Returns the type name of the node type.
  static StrRef NodeTypeName(NodeKind Kind);
  /// Returns the type name of the node.
  static StrRef Type(NodeT Node);
  /// Returns if the node type has a name.
  static bool HasData(NodeT Node);
  /// Returns if the node type has data.
  static bool HasName(NodeT Node);
  /// Returns if the node type has children.
  static bool HasChildren(NodeT Node);
  /// Returns if the node type has attributes.
  static bool HasAttributes(NodeT Node);

  static std::pair<StrRef, StrRef> SplitNodeName(StrRef IName) {
    auto [NameOrNs, Name] = IName.split(':');
    if (Name.empty())
      return {"", NameOrNs};
    else
      return {NameOrNs, Name};
  }
  static std::pair<StrRef, StrRef>
   SplitNodeName(const XMLBase* Node) {
    if (!Node || !Node->name_data()) [[unlikely]]
      return {};
    return SplitNodeName(Node->name());
  }

  /// Root for dumping.
  void dump(int InitialIndent = 0);

private:
  void print(NodeT Node);
  void printIndividual(NodeT Node);
  void printHead(NodeT Node);
  void printTail(NodeT Node);

  void printNode_element(NodeT Node);
  void printNode_data(NodeT Node);
  void printNode_cdata(NodeT Node);
  void printNode_comment(NodeT Node);
  void printNode_declaration(NodeT Node);
  void printNode_doctype(NodeT Node);
  void printNode_pi(NodeT Node);

  void printName(NodeT Node);
  void printAttrName(const XMLAttribute* Attr);
  void printAttr(const XMLAttribute* Attr);
  void printAttrs(NodeT Node);
  void printDOCTYPEData(StrRef Data,
                        Option<raw_ostream&> IOS = std::nullopt);
  void printType(NodeT Node, StrRef Extra = "");
  void printErr(NodeT Node, StrRef Err = "err");

  void putName(StrRef Name);
  void putNs(StrRef NS);
  void putAttr(StrRef Attr);
  void putAttrNs(StrRef Attr);
  void putString(StrRef Str);
  void putCDATA(StrRef CDATA);
  void putSplit(char Split);
  void putComment(StrRef Comment);
  void putData(StrRef Data);
  void putEntity(StrRef Data);

  bool expectData(NodeT Node, StrRef Err = "err");
  bool expectName(NodeT Node, StrRef Err = "err");
};
} // namespace `anonymous`

StrRef XMLDumper::NodeTypeName(NodeKind Kind) {
  static constexpr StringLiteral Names[] {
    "document", "element", "data",
    "CDATA",
    "comment",
    "declaration",
    "DOCTYPE", "PI"
  };

  const int Off = exi::to_underlying(Kind);
  if (Off < int(NodeKind::node_last))
    return Names[Off];
  return XMLDumper::knode_unknown;
}

StrRef XMLDumper::Type(NodeT Node) {
  return Node ? NodeTypeName(Node->type()) : knode_null;
}

bool XMLDumper::HasName(NodeT Node) {
  if (!Node)
    return false;
  const MMatch M(Node->type());
  if (M.isnt(xml::node_element, xml::node_pi))
    return false;
  return !Node->name().empty();
}

bool XMLDumper::HasData(NodeT Node) {
  if (!Node)
    return false;
  const MMatch M(Node->type());
  if (M.is(xml::node_document, xml::node_declaration))
    return false;
  return !Node->value().empty();
}

bool XMLDumper::HasChildren(NodeT Node) {
  return Node ? !!Node->first_node() : false;
}

bool XMLDumper::HasAttributes(NodeT Node) {
  return Node ? !!Node->first_attribute() : false;
}

bool XMLDumper::expectName(NodeT Node, StrRef Err) {
  if (!HasName(Node)) [[unlikely]] {
    printErr(Node, Err);
    return true;
  }
  return false;
}
bool XMLDumper::expectData(NodeT Node, StrRef Err) {
  if (!HasData(Node)) [[unlikely]] {
    printErr(Node, Err);
    return true;
  }
  return false;
}

//////////////////////////////////////////////////////////////////////////
// Atoms

void XMLDumper::putName(StrRef Name) {
  WithColor Save(OS, COLOR_name);
  Save << Name;
}
void XMLDumper::putNs(StrRef NS) {
  WithColor Save(OS, COLOR_ns);
  Save << NS;
}
void XMLDumper::putAttr(StrRef Attr) {
  WithColor Save(OS, COLOR_attr);
  Save << Attr;
}
void XMLDumper::putAttrNs(StrRef NS) {
  WithColor Save(OS, COLOR_attrns);
  Save << NS;
}
void XMLDumper::putString(StrRef Str) {
  WithColor Save(OS, COLOR_string);
  Save << '"' << Str << '"';
}
void XMLDumper::putCDATA(StrRef CDATA) {
  WithColor Save(OS, COLOR_cdata);
  Save << CDATA;
}
void XMLDumper::putSplit(char Split) {
  WithColor Save(OS, COLOR_split);
  Save << Split;
}
void XMLDumper::putComment(StrRef Comment) {
  WithColor Save(OS, COLOR_comment);
  Save << Comment;
}
void XMLDumper::putData(StrRef Data) {
  WithColor Save(OS, COLOR_data);
  Save << Data;
}
void XMLDumper::putEntity(StrRef Data) {
  Data.consume_pinch("&", ";");
  WithColor Save(OS, COLOR_entity);
  putSplit('&');
  Save << Data;
  putSplit(';');
}

//////////////////////////////////////////////////////////////////////////
// Fragments

void XMLDumper::printType(NodeT Node, StrRef Extra) {
  WithColor Save(OS, COLOR_comment);
  Save << '@' << Type(Node) << Extra;
}

void XMLDumper::printErr(NodeT Node, StrRef Val) {
  WithColor Save(OS, BRIGHT_RED);
  Save << '@' << Val << "::" << Type(Node);
}

void XMLDumper::printName(NodeT Node) {
  if (!HasName(Node)) {
    printErr(Node, "no-name");
    return;
  }

  auto [Ns, Name] = SplitNodeName(Node);
  if (!Ns.empty()) {
    this->putNs(Ns);
    this->putSplit(':');
  }
  this->putName(Name);
}

void XMLDumper::printAttrName(const XMLAttribute* Attr) {
  if (!Attr || Attr->name().empty()) [[unlikely]] {
    WithColor Save(OS, BRIGHT_RED);
    Save << "@no-attr-name::attribute";
    return;
  }

  auto [Ns, Name] = SplitNodeName(Attr);
  if (!Ns.empty()) {
    this->putAttrNs(Ns);
    this->putSplit(':');
  }
  this->putAttr(Name);
}

void XMLDumper::printAttr(const XMLAttribute* Attr) {
  printAttrName(Attr);
  putSplit('=');
  putString(Attr->value());
}

static bool SortAttrsQName(const XMLAttribute* LHS, const XMLAttribute* RHS) {
  exi_invariant(LHS && RHS);
  StrRef LHS_FullName = LHS->name(), RHS_FullName = RHS->name();
  
  const bool LHS_xmlns = LHS_FullName.starts_with("xmlns");
  if (LHS_xmlns && LHS_FullName.size() == 5)
    return false;
  
  const bool RHS_xmlns = RHS_FullName.starts_with("xmlns");
  if (RHS_xmlns && RHS_FullName.size() == 5)
    return true;

  auto [LHSNs, LHSName] = XMLDumper::SplitNodeName(LHS_FullName);
  auto [RHSNs, RHSName] = XMLDumper::SplitNodeName(RHS_FullName);
  
  const int NsCmp = LHSNs.compare(RHSNs);
  if (NsCmp == 0) {
    // Same namespace.
    if (LHSNs == "xsi")
      return (LHSName != "type");
    return (LHSName < RHSName);
  }
  
  // Namespaces are different.
  if (LHS_xmlns || RHS_xmlns)
    // Check our cached xmlns lookup.
    return RHS_xmlns;
  
  if (LHSNs == "xsi")
    return false;
  if (RHSNs == "xsi")
    return true;
  
  // Return if less than in cached check.
  return (NsCmp < 0);
}

static bool SortAttrs(const XMLNode* Node,
                      SmallVecImpl<const XMLAttribute*>& Attrs) {
  if (!XMLDumper::HasAttributes(Node))
    return false;
  
  auto* Curr = Node->first_attribute();
  while (Curr) {
    Attrs.push_back(Curr);
    Curr = Curr->next_attribute();
  }
  
  std::sort(Attrs.begin(), Attrs.end(),
  [] (auto LHS, auto RHS) -> bool {
    bool Result;
    INLINE_STMT Result = SortAttrsQName(LHS, RHS);
    return !Result;
  });
  
  return true;
}

void XMLDumper::printAttrs(NodeT Node) {
  SmallVec<const XMLAttribute*> Attrs;
  if (!SortAttrs(Node, Attrs))
    return;
  for (auto*& Attr : Attrs) {
    printAttr(Attr);
    if (Attr == Attrs.back())
      break;
    OS << ' ';
  }
}

void XMLDumper::printDOCTYPEData(StrRef Data, Option<raw_ostream&> IOS) {
  raw_ostream& OS
    = IOS.value_or(this->OS);
 {
  ScopedSave S(Indent);
  ++Indent;

  SmallVec<StrRef, 4> Split;
  Data.split(Split, '\n', -1, false);

  WithColor Save(OS, COLOR_data);
  for (StrRef S : Split) {
    S = S.rtrim();
    if (S.empty())
      continue;
    Save << '\n' << Indent << S;
  }
 }
  OS << '\n' << Indent;
}

//////////////////////////////////////////////////////////////////////////
// Elements

void XMLDumper::printNode_element(NodeT Node) {
  OS << '<';
  this->printName(Node);
  if (HasAttributes(Node)) {
    OS << ' ';
    printAttrs(Node);
  }
  if (!HasChildren(Node))
    OS << '/';
  OS << ">\n";
}

void XMLDumper::printNode_data(NodeT Node) {
  if (expectData(Node, "no-data"))
    return;
  
  StrRef Data = Node->value().trim().rtrim();
  if (Data.empty())
    return;
  
  while (!Data.empty()) {
    auto [Front, Back] = Data.split('&');
    if (Back.empty())
      break;
    
    StrRef Entity;
    std::tie(Entity, Data) = Back.split(';');
    putEntity(Entity);
  }

  putData(Data);
  OS << '\n';
}

void XMLDumper::printNode_cdata(NodeT Node) {
  if (expectData(Node, "no-CDATA"))
    return;
  
  SmallStr<32> Storage;
  raw_svector_ostream OS(Storage);

  OS << "<![CDATA[";
  printDOCTYPEData(Node->value(), OS);
  OS << "]]>\n";

  putCDATA(Storage.str());
}

void XMLDumper::printNode_comment(NodeT Node) {
  if (expectData(Node, "no-comment"))
    return;
  SmallStr<32> Str;
  exi::wrap_stream(Str)
    << "<!--" << Node->value() << "-->\n";
  putComment(Str.str());
}

void XMLDumper::printNode_declaration(NodeT Node) {
  if (!HasAttributes(Node)) {
    printErr(Node, "no-decl-attrs");
    return;
  }

  OS << "<?";
  putName("xml ");
  printAttrs(Node);
  OS << "?>\n";
}

void XMLDumper::printNode_doctype(NodeT Node) {
  if (expectData(Node, "no-data"))
    return;
  
  OS << "<!";
  putAttr("DOCTYPE ");
  auto [Pre, Data] = Node->value().split('[');

  if (Data.empty())
    printErr(Node, "no-opening-brace");
  else if (!Data.consume_back("]"))
    printErr(Node, "no-closing-brace");
  else {
    { WithColor(OS, COLOR_dtname) << Pre; }
    putSplit('[');
    printDOCTYPEData(Data);
    putSplit(']');
  }

  OS << ">\n";
}

void XMLDumper::printNode_pi(NodeT Node) {
  if (expectName(Node, "no-PI-target"))
    return;
  if (expectData(Node, "no-PI-directives"))
    return;
  
  SmallStr<32> Storage;
  raw_svector_ostream OS(Storage);

  OS << "<?" << Node->name();
  SmallVec<StrRef, 2> PIVec;
  Node->value().split(PIVec, ' ', -1, false);
  for (StrRef PI : PIVec)
    OS << ' ' << PI;
  OS << "?>\n";

  WithColor(this->OS, COLOR_dtname) << Storage.str();
}

//////////////////////////////////////////////////////////////////////////
// Impl

void XMLDumper::dump(int InitialIndent) {
  WithColor Save(OS, COLOR_default);
  ScopedSave S(Indent);
  Indent = InitialIndent;
  this->print(TopLevel.first_node());
}

void XMLDumper::print(NodeT Node) {
  while (Node) {
    printIndividual(Node);
    if (!Node->parent())
      break;
    Node = Node->next_sibling();
  }
}

void XMLDumper::printIndividual(NodeT Node) {
  printHead(Node);

  if (HasChildren(Node)) {
    ScopedSave S(Indent);
    ++Indent;
    print(Node->first_node());
  }
  
  printTail(Node);
}

void XMLDumper::printHead(NodeT Node) {
  using namespace xml;
  exi_assert(Node != nullptr);
  if (Node == nullptr)
    return;
  
  OS << Indent;
  if (DebugPrint)
    printType(Node, ": ");
  switch (Node->type()) {
  case node_element:
    return printNode_element(Node);
  case node_data:
    return printNode_data(Node);
  case node_cdata:
    return printNode_cdata(Node);
  case node_comment:
    return printNode_comment(Node);
  case node_declaration:
    return printNode_declaration(Node);
  case node_doctype:
    return printNode_doctype(Node);
  case node_pi:
    return printNode_pi(Node);
  default:
    return;
  }
}

void XMLDumper::printTail(NodeT Node) {
  if (HasChildren(Node)) {
    OS << Indent << "</";
    this->printName(Node);
    OS << ">\n";
  }
}

//////////////////////////////////////////////////////////////////////////
// ...

static Option<XMLDocument&> TryLoad(XMLManager& Mgr, const Twine& Filepath) {
  if (Mgr.getOptXMLDocument(Filepath, errs()))
    return Mgr.getOptXMLDocument(Filepath, errs());
  return std::nullopt;
}

static usize ReserveSize(XMLManager& Mgr, const Twine& Filepath) {
  return Mgr.getOptXMLRef(Filepath)
    .expect("bruh??")
    .getBufferRef()
    .getBufferSize();
}

static void HandleErr(XMLManager& Mgr, StrRef Name, raw_ostream& OS) {
  auto RefOrErr = Mgr.getXMLRef(Name);
  if (Error Err = RefOrErr.takeError()) {
    logAllUnhandledErrors(std::move(Err), OS);
    return;
  }
  if (!RefOrErr->hasEntry()) {
    OS << format("Entry for '{}' was never provided.", Name);
    return;
  }

  auto DocOrErr = Mgr.getXMLDocument(Name);
  if (Error Err = DocOrErr.takeError()) {
    logAllUnhandledErrors(std::move(Err), OS);
    return;
  }
}

void root::FullXMLDump(exi::XMLManager& Mgr,
                       const exi::Twine& Filepath,
                       exi::Option<raw_ostream&> InOS,
                       bool DbgPrintTypes) {
  SmallStr<80> Storage;
  StrRef Name = Filepath.toStrRef(Storage);

  raw_ostream& OutS = InOS.value_or(outs());
  const bool OSProvided = InOS.has_value();

  if (auto Doc = TryLoad(Mgr, Name)) {
    if (!OSProvided) {
      OutS << "'" << Name << "':\n";
      OutS.flush();
    }

    SmallStr<512> PrintBuf;
    PrintBuf.reserve(ReserveSize(Mgr, Name));
    raw_svector_ostream OS(PrintBuf);
    OS.enable_colors(outs().has_colors());

    XMLDumper Dumper(*Doc, 2, OS);
    Dumper.DebugPrint = DbgPrintTypes;
    Dumper.dump(/*InitialIndent=*/ OSProvided ? 0 : 1);

    OutS << PrintBuf.str() << '\n';
  } else {
    HandleErr(Mgr, Name, errs());
    errs() << '\n';
  }
  OutS.changeColor(raw_ostream::RESET).flush();
}
