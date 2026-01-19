//===- exi/Basic/XMLDumper.cpp --------------------------------------===//
//
// Copyright (C) 2024-2026 Ninefold
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

#include <exi/Basic/XMLDumper.hpp>
#include <core/Common/SmallStr.hpp>
#include <core/Common/IntrusiveRefCntPtr.hpp>
#include <core/Common/MMatch.hpp>
#include <core/Common/StringExtras.hpp>
#include <core/Common/STLExtras.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/MemoryBuffer.hpp>
#include <core/Support/Logging.hpp>
#include <core/Support/ScopedSave.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/Except.hpp>
#include <exi/Basic/XMLManager.hpp>
#include <exi/Basic/XMLContainer.hpp>
#include <exi/Encode/DTDParser.hpp>
#include <algorithm>
#include <rapidxml.hpp>

#define DEBUG_TYPE "XMLDumper"

using namespace exi;

using PreserveCDATAKind = XMLCoderOptions::PreserveCDATAKind;

namespace {
class XMLDumper {
  static constexpr StrRef knode_unknown = "UNKNOWN-TYPE"; 
  static constexpr StrRef knode_null = "NULL-TYPE"; 

  XMLDocument& TopLevel;
  raw_ostream& OS;
  exi::indent Indent;

public:
  using enum PreserveCDATAKind;
  using enum raw_ostream::Colors;
  using Colors = raw_ostream::Colors;

  bool DebugPrint = false;
  /// If `true`, namespaces will be in document order.
  /// Otherwise, namespaces will be in shortlex order.
  bool ConformingSort = false;

  bool PreserveComments = true;
  bool PreserveDTDs     = true;
  bool PreservePIs      = true;
  bool PreservePrefixes = true;
  bool PreserveDecl     = false;
  PreserveCDATAKind PreserveCDATA = CDATA_PRESERVE;

  Colors COLOR_default = CYAN;
  Colors COLOR_name    = BRIGHT_CYAN;
  Colors COLOR_dtname  = BRIGHT_YELLOW;
  Colors COLOR_ns      = BLUE;
  Colors COLOR_attr    = BRIGHT_MAGENTA;
  Colors COLOR_attrns  = BRIGHT_BLUE;
  Colors COLOR_string  = BRIGHT_GREEN;
  Colors COLOR_cdata   = BRIGHT_GREEN;
  Colors COLOR_split   = BRIGHT_BLACK;
  Colors COLOR_comment = BRIGHT_BLACK;
  Colors COLOR_entity  = BRIGHT_RED;
  Colors COLOR_data    = BRIGHT_WHITE;
  Colors COLOR_error   = BRIGHT_RED;
  Colors COLOR_debug   = BRIGHT_BLACK;

public:
  using NodeT = const XMLNode*;
  XMLDumper(XMLDocument& Doc, int IndentLevel = 2,
            Option<raw_ostream&> OS = std::nullopt) :
   TopLevel(Doc), OS(OS.value_or(nulls())),
   Indent(0, IndentLevel) {
  }

  void setup(const XMLDumpOptions& Opts) {
    /// Preserve.*
    PreserveComments  = Opts.Preserve.Comments;
    PreserveDTDs      = Opts.Preserve.DTDs;
    PreservePIs       = Opts.Preserve.PIs;
    PreservePrefixes  = Opts.Preserve.Prefixes;
    PreserveDecl      = Opts.PreserveDeclaration.value_or(PreservePIs);
    PreserveCDATA     = Opts.PreserveCDATA;
    /// Misc
    ConformingSort    = Opts.Conforming;
    DebugPrint        = Opts.Debug;
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

  static std::pair<StrRef, StrRef> SplitNodeName(StrRef Name) {
    return Name.split_back(':');
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
  void printAttrsArr(const auto& Attrs);
  void printAttrsSlxOrd(NodeT Node); // Shortlex Order
  void printAttrsDocOrd(NodeT Node); // Document Order
  void printAttrs(NodeT Node);
  void printCDATAData(StrRef Data, Option<raw_ostream&> IOS = std::nullopt);
  template <bool Escaped>
  void printCDATABlock(StrRef Data, Option<raw_ostream&> IOS = std::nullopt);
  void printDOCTYPEDecl(StrRef Data, raw_ostream& OS);
  void printDOCTYPEData(StrRef Data, Option<raw_ostream&> IOS = std::nullopt);
  void printPIData(StrRef Name, StrRef Data);
  void printDiscarded(NodeT Node, StrRef Extra = "");
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

  inline bool preserved(NodeT Node);
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

EXI_INLINE bool XMLDumper::preserved(NodeT Node) {
  using namespace xml;
  exi_invariant(Node != nullptr);
  switch (Node->type()) {
  case node_comment:
    return PreserveComments;
  case node_declaration:
    return PreserveDecl;
  case node_doctype:
    return PreserveDTDs;
  case node_pi:
    return PreservePIs;
  default:
    return true;
  }
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
  WithColor Save(OS, COLOR_debug);
  Save << '@' << Type(Node) << Extra;
}

void XMLDumper::printDiscarded(NodeT Node, StrRef Extra) {
  WithColor Save(OS, COLOR_debug);
  Save << '@' << Extra << "::" << Type(Node);
}

void XMLDumper::printErr(NodeT Node, StrRef Val) {
  WithColor Save(OS, COLOR_error);
  Save << '@' << Val << "::" << Type(Node);
}

void XMLDumper::printName(NodeT Node) {
  if (!HasName(Node)) {
    printErr(Node, "no-name");
    return;
  }

  auto [Ns, Name] = SplitNodeName(Node);
  // TODO: Check if prefix is xml/xsi
  if (PreservePrefixes && !Ns.empty()) {
    this->putNs(Ns);
    this->putSplit(':');
    if EXI_UNLIKELY(Name.empty()) {
      WithColor(OS, BRIGHT_RED) << "@no-name";
      return;
    }
  }
  this->putName(Name);
}

void XMLDumper::printAttrName(const XMLAttribute* Attr) {
  if (!Attr || Attr->name().empty()) [[unlikely]] {
    WithColor(OS, BRIGHT_RED) << "@no-attr-name::attribute";
    return;
  }

  if (Attr->id_kind() == xml::IK_AnonNS) {
    this->putAttrNs("xmlns");
    return;
  }

  auto [Ns, Name] = SplitNodeName(Attr);
  // TODO: Check if prefix is xml/xsi
  if (PreservePrefixes && !Ns.empty()) {
    this->putAttrNs(Ns);
    this->putSplit(':');
    if EXI_UNLIKELY(Name.empty()) {
      WithColor(OS, BRIGHT_RED) << "@no-attr-name";
      return;
    }
  }
  this->putAttr(Name);
}

void XMLDumper::printAttr(const XMLAttribute* Attr) {
  printAttrName(Attr);
  putSplit('=');
  putString(Attr->value());
}

void XMLDumper::printAttrsArr(const auto& Attrs) {
  if (PreservePrefixes) {
    for (const XMLAttribute* Attr : Attrs) {
      OS << ' ';
      printAttr(Attr);
    }
  } else {
    for (const XMLAttribute* Attr : Attrs) {
      if (Attr->is_namespace())
        continue;
      OS << ' ';
      printAttr(Attr);
    }
  }
}

static bool SortAttrsXmlns(const XMLAttribute* LHS, const XMLAttribute* RHS) {
  StrRef LHS_NS = LHS->name().drop_front(6),
         RHS_NS = RHS->name().drop_front(6);
  return LHS_NS.compare_shortlex(RHS_NS) < 0;
}

static bool SortAttrsNormal(const XMLAttribute* LHS, const XMLAttribute* RHS) {
  auto [LHSNs, LHSName] = XMLDumper::SplitNodeName(LHS->name());
  auto [RHSNs, RHSName] = XMLDumper::SplitNodeName(RHS->name());
  const int NsCmp = LHSNs.compare(RHSNs);
  if (NsCmp == 0)
    // Same namespace.
    return (LHSName < RHSName);
  // Return if less than in cached check.
  return (NsCmp < 0);
}

static bool SortAttrsQName(const XMLAttribute* LHS, const XMLAttribute* RHS) {
  exi_invariant(LHS && RHS);
  if (LHS->id_rank() != RHS->id_rank())
    return LHS->id_rank() < RHS->id_rank();
  
  using enum xml::IdentifierKind;
  switch (LHS->id_rank()) {
    case IK_Name:
      tail_return SortAttrsNormal(LHS, RHS);
    case IK_NamedNS:
      tail_return SortAttrsXmlns(LHS, RHS);
    default:
      Throw<argument_error>("Invalid IdentifierKind!");
  }
}

static bool SortAttrs(const XMLNode* Node,
                      SmallVecImpl<const XMLAttribute*>& Attrs,
                      SmallVecImpl<const XMLAttribute*>* NS = nullptr) {
  exi_invariant(XMLDumper::HasAttributes(Node));
  bool HasSplitNS = NS && (&Attrs != NS);
  if (NS == nullptr)
    NS = &Attrs;

  auto* Curr = Node->first_attribute();
  while (Curr) {
    if (Curr->is_name())
      Attrs.push_back(Curr);
    else
      NS->push_back(Curr);
    Curr = Curr->next_attribute();
  }

  if (Attrs.size() <= 1)
    return !(Attrs.empty() && NS->empty());
  
  std::sort(Attrs.begin(), Attrs.end(),
  [] (auto* LHS, auto* RHS) -> bool {
    // TODO: Add variant that sorts by LN, then URI?
    return !SortAttrsQName(LHS, RHS);
  });
  
  return true;
}

void XMLDumper::printAttrsSlxOrd(NodeT Node) {
  SmallVec<const XMLAttribute*, 16> Attrs;
  if (!SortAttrs(Node, Attrs))
    return;
  printAttrsArr(Attrs);
}
void XMLDumper::printAttrsDocOrd(NodeT Node) {
  SmallVec<const XMLAttribute*, 4> NS;
  SmallVec<const XMLAttribute*, 12> Attrs;
  if (!SortAttrs(Node, Attrs, &NS))
    return;
  auto NSAndAttrs = exi::concat<const XMLAttribute*>(NS, Attrs);
  printAttrsArr(NSAndAttrs);
}

void XMLDumper::printAttrs(NodeT Node) {
  if (!XMLDumper::HasAttributes(Node))
    return;
  if (ConformingSort)
    tail_return printAttrsDocOrd(Node);
  else
    tail_return printAttrsSlxOrd(Node);
}

void XMLDumper::printCDATAData(StrRef Data, Option<raw_ostream&> IOS) {
  constexpr auto Filter = StrRef::filter_t::FromChars("\n\r\v\f");
  raw_ostream& OS = IOS.value_or(this->OS);
 {
  WithColor Save(OS, COLOR_data);
  ScopedSave S(Indent);
  ++Indent;

  std::pair<StrRef, StrRef> Str = getToken(Data, Filter);
  while (!Str.first.empty()) {
    StrRef Out = Str.first.ltrim(' ');
    if (!Out.empty())
      OS << '\n' << Indent << Out;
    Str = getToken(Str.second, Filter);
  }
 }
  OS << '\n' << Indent;
}

template <bool Escaped>
void XMLDumper::printCDATABlock(StrRef Data, Option<raw_ostream&> IOS) {
  constexpr auto Filter = StrRef::filter_t::FromChars("\n\r\v\f");
  exi_invariant(PreserveCDATA);
  raw_ostream& OS = IOS.value_or(this->OS);
 {
  WithColor Save(OS, COLOR_data);
  std::pair<StrRef, StrRef> Str = getToken(Data, Filter);

  auto Sep = fmt::format("\n{}", Indent);
  ListSeparator LS(Sep);
  while (!Str.first.empty()) {
    StrRef Out = Str.first.ltrim(' ');
    if (!Out.empty()) {
      if constexpr (Escaped)
        OS << LS << escape::xml(Out);
      else
        OS << LS << Out;
    }
    Str = getToken(Str.second, Filter);
  }
 }
}

enum DTDeclKind : int {
  Decl_ELEMENT,
  Decl_ATTLIST,
  Decl_ENTITY,
  Decl_NOTATION,
  Decl_INVALID = -1,
};

void XMLDumper::printDOCTYPEDecl(StrRef Tok, raw_ostream& OS) {
  if (!Tok.consume_pinch("<!", ">")) {
    WithColor Save(OS, BRIGHT_RED);
    Save << escape(Tok);
    return;
  }

  auto PeekNextTokenChar = [&Tok] () -> char {
    usize Start = Tok.find_first_not_of(DTDParser::kDelimiter);
    return Start != StrRef::npos ? Tok[Start] : '\0';
  };
  auto TakeToken = [&Tok] () -> StrRef {
    usize Start = Tok.find_first_not_of(DTDParser::kDelimiter);
    if (Start == StrRef::npos) {
      Tok = "";
      return "";
    }

    const char C = Tok[Start];
    if (mmatch_value(C).is('\"', '\'', '(')) {
      const char F = (C == '(') ? ')' : C;
      usize End = Tok.find_first_of(F, Start + 1);
      End += (End != StrRef::npos);
      StrRef Out = Tok.slice(Start, End);
      Tok = Tok.substr(End);
      return Out;
    }

    usize End = Tok.find_first_of(DTDParser::kDelimiter, Start);
    StrRef Out = Tok.slice(Start, End);
    Tok = Tok.substr(End);
    return Out;
  };

  Tok = Tok.trim();
  if (Tok.empty()) {
    WithColor Save(OS, BRIGHT_RED);
    Save << "<!@empty-decl::DOCTYPE>";
    return;
  }

  StrRef TypeName = TakeToken();
  auto ty = StringSwitch<DTDeclKind>(TypeName)
    .Case("ELEMENT",  Decl_ELEMENT)
    .Case("ATTLIST",  Decl_ATTLIST)
    .Case("ENTITY",   Decl_ENTITY)
    .Case("NOTATION", Decl_NOTATION)
    .Default(Decl_INVALID);

  if EXI_UNLIKELY(ty == Decl_INVALID) {
    WithColor Save(OS, BRIGHT_RED);
    Save << "<!@invalid-type::" << TypeName << ">";
    return;
  }

  OS << "<!";
  putAttr(TypeName);

  if (ty == Decl_ELEMENT) {
    StrRef Name = TakeToken();
    // Check if PEDecl
    if (Name == "%") {
      WithColor(OS, COLOR_split) << " %";
      Name = TakeToken();
    }
    // Name
    WithColor(OS, COLOR_name) << ' ' << Name;
  }

 {
  WithColor Save(OS, BRIGHT_WHITE);
  while (!Tok.empty()) {
    StrRef S = TakeToken();
    if (S.empty())
      break;
    OS << ' ';

    if (S[0] == '\"' || S[0] == '\'')
      WithColor(OS, COLOR_string) << S;
    else if (S[0] == '#')
      WithColor(OS, COLOR_entity) << S;
    else if (S == "%")
      putSplit('%');
    else if (isUpper(S))
      putAttr(S);
    else if (isAlpha(S[0]))
      putName(S);
    else
      OS << S;
  }
 }

  OS << ">";
}

void XMLDumper::printDOCTYPEData(StrRef Data, Option<raw_ostream&> IOS) {
  raw_ostream& OS = IOS.value_or(this->OS);
 {
  ScopedSave S(Indent);
  ++Indent;

  SmallVec<StrRef, 4> Tokens;
  DTDParser::SplitDTText(Data, Tokens);

  for (StrRef Tok : Tokens) {
    OS << '\n' << Indent;
    if (Tok[1] == '!') {
      if (Tok[2] == '-') {
        WithColor Save(OS, COLOR_comment);
        Save << Tok;
      } else
        printDOCTYPEDecl(Tok, OS);
    } else /*Tok[1] == '?'*/ {
      if (!Tok.consume_pinch("<?", "?>")) {
        WithColor Save(OS, BRIGHT_RED);
        Save << escape(Tok);
        continue;
      }
      auto [Name, Text] = getToken(Tok);
      printPIData(Name, Text.trim());
    }
  }
 }
  OS << '\n' << Indent;
}

void XMLDumper::printPIData(StrRef Name, StrRef Data) {
  SmallStr<32> Storage;
  raw_svector_ostream OS(Storage);

  OS << "<?" << Name;
  SmallVec<StrRef, 2> PIVec;
  Data.split(PIVec, ' ', -1, false);
  for (StrRef PI : PIVec)
    OS << ' ' << PI;
  OS << "?>";

  WithColor(this->OS, COLOR_dtname) << Storage.str();
}

//////////////////////////////////////////////////////////////////////////
// Elements

void XMLDumper::printNode_element(NodeT Node) {
  OS << '<';
  this->printName(Node);
  if (HasAttributes(Node)) {
    printAttrs(Node);
  }
  if (!HasChildren(Node))
    OS << '/';
  OS << ">\n";
}

void XMLDumper::printNode_data(NodeT Node) {
  if (expectData(Node, "no-data"))
    return;
  
  auto PutData = [this] (StrRef Data) {
    while (!Data.empty()) {
      auto [Front, Back] = Data.split('&');
      if (Back.empty())
        break;
      
      StrRef Entity;
      std::tie(Entity, Data) = Back.split(';');

      this->putData(Front);
      this->putEntity(Entity);
    }

    this->putData(Data);
  };
  
  StrRef Data = Node->value().trim();
  if (Data.empty())
    return;
  
  constexpr auto Filter = StrRef::filter_t::FromChars("\n\r\v\f");
  std::pair<StrRef, StrRef> Str = getToken(Data, Filter);
  
  auto Sep = fmt::format("\n{}", Indent);
  ListSeparator LS(Sep);
  while (!Str.first.empty()) {
    StrRef Out = Str.first.ltrim(' ');
    if (!Out.empty()) {
      OS << LS;
      PutData(Out);
    }
    Str = getToken(Str.second, Filter);
  }

  OS << '\n';
}

void XMLDumper::printNode_cdata(NodeT Node) {
  if (expectData(Node, "no-CDATA"))
    return;
  
  SmallStr<32> Storage;
  raw_svector_ostream OS(Storage);
  StrRef Data = Node->value().trim();

  if (PreserveCDATA == CDATA_PRESERVE) {
    OS << "<![CDATA[";
    printCDATAData(Node->value(), OS);
    OS << "]]>\n";
  } else {
    if (PreserveCDATA == CDATA_ESCAPE)
      printCDATABlock<true>(Node->value(), OS);
    else
      printCDATABlock<false>(Node->value(), OS);
    OS << '\n';
  }

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
  
  auto PrintDT = [this] (DoctypeEvent& DT) {
    { WithColor(OS, COLOR_dtname) << DT.name(); }
    if (DT.Kind == DTK_None)
      return;
    
    if (DT.Kind == DTK_System) {
      OS << " SYSTEM ";
      putString(DT.systemID());
    } else if (DT.Kind == DTK_Public) {
      OS << " PUBLIC ";
      putString(DT.publicID());
      OS << ' ';
      putString(DT.systemID());
    }

    StrRef Text = DT.text();
    if (!Text.empty()) {
      OS << " [";
      printDOCTYPEData(Text);
      OS << "]";
    }
  };

  auto DTOrErr = DTDParser::CreateDTEvent(Node->value());
  if (DTOrErr.is_err())
    printErr(Node, "invalid-doctype");
  else
    PrintDT(*DTOrErr);

  OS << ">\n";
}

void XMLDumper::printNode_pi(NodeT Node) {
  if (expectName(Node, "no-PI-target"))
    return;
  if (expectData(Node, "no-PI-directives"))
    return;
  
  printPIData(Node->name(), Node->value());
  OS << '\n';
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
  
  if (!preserved(Node)) {
    if (DebugPrint) {
      OS << Indent;
      printDiscarded(Node, "not-preserved");
    }
    return;
  }
  
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

static int CalcInitialIndent(const XMLDumpOptions& Opts) {
  if (Opts.InitialIndent)
    return *Opts.InitialIndent;
  if (auto OS = Opts.OS) {
    if (!OS->is_displayed())
      return 0;
  }
  return 1;
}

void XMLDump::full(XMLDocument& Doc, const XMLDumpOptions& Opts) {
  raw_ostream& OutS = Opts.OS.value_or(outs());
  const bool OSProvided = Opts.OS.has_value();
  const int InitialIndent = CalcInitialIndent(Opts);

  if (!OSProvided)
    OutS.flush();

  SmallStr<512> PrintBuf;
  raw_svector_ostream OS(PrintBuf);
  OS.enable_colors(OutS.has_colors());

  XMLDumper Dumper(Doc, Opts.IndentScale, OS);
  Dumper.setup(Opts);
  Dumper.dump(InitialIndent);

  OutS << PrintBuf.str() << '\n';
  OutS.changeColor(raw_ostream::RESET).flush();
}

void XMLDump::full(XMLManager& Mgr,
                   const Twine& Filepath,
                   const XMLDumpOptions& Opts) {
  SmallStr<80> Storage;
  StrRef Name = Filepath.toStrRef(Storage);

  raw_ostream& OutS = Opts.OS.value_or(outs());
  const bool OSProvided = Opts.OS.has_value();
  const int InitialIndent = CalcInitialIndent(Opts);

  if (auto Doc = TryLoad(Mgr, Name)) {
    if (!OSProvided) {
      OutS << "'" << Name << "':\n";
      OutS.flush();
    }

    SmallStr<512> PrintBuf;
    PrintBuf.reserve(ReserveSize(Mgr, Name));
    raw_svector_ostream OS(PrintBuf);
    OS.enable_colors(OutS.has_colors());

    XMLDumper Dumper(*Doc, Opts.IndentScale, OS);
    Dumper.setup(Opts);
    Dumper.dump(InitialIndent);

    OutS << PrintBuf.str() << '\n';
  } else {
    HandleErr(Mgr, Name, errs());
    errs() << '\n';
  }
  OutS.changeColor(raw_ostream::RESET).flush();
}
