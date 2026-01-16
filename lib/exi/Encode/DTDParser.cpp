//===- exi/Encode/DTDParser.cpp --------------------------------------===//
//
// Copyright (C) 2026 Ninefold
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
///
/// \file
/// This file implements simple parsing for DOCTYPE events.
///
//===----------------------------------------------------------------===//

#include <exi/Encode/DTDParser.hpp>
#include <core/Common/Option.hpp>
#include <core/Common/SmallVec.hpp>
#include <Common/StrRef-inl.hpp>

#define DEBUG_TYPE "DTDParser"

using namespace exi;

/// doctypedecl         ::=       '<!DOCTYPE' S Name (S ExternalID)? S? ('[' intSubset ']' S?)? '>'    [VC: Root Element Type]
/// intSubset           ::=       (markupdecl | DeclSep)*
/// DeclSep             ::=       PEReference | S
/// markupdecl          ::=       elementdecl | AttlistDecl | EntityDecl | NotationDecl | PI | Comment
/// 
/// Comment             ::=       '<!--' ((. - "-") | ('-' [^-]))* '-->'
/// PI                  ::=       '<?' PITarget (S (.* - (.* '?>' .*)))? '?>'
/// PITarget            ::=       Name - [XMLxml]
/// 
/// S                   ::=       (#x20 | #x9 | #xD | #xA)+
/// NameStartChar       ::=       [A-Za-z] | [:_] | [#xC0-#xD6] | [#xD8-#xF6] | [#xF8-#x2FF] | [#x370-#x37D] | [#x37F-#x1FFF] | [#x200C-#x200D] | [#x2070-#x218F] | [#x2C00-#x2FEF] | [#x3001-#xD7FF] | [#xF900-#xFDCF] | [#xFDF0-#xFFFD] | [#x10000-#xEFFFF]
/// NameChar            ::=       NameStartChar | "-" | "." | [0-9] | #xB7 | [#x0300-#x036F] | [#x203F-#x2040]
/// Name                ::=       NameStartChar (NameChar)*
/// Nmtoken             ::=       (NameChar)+
/// 
/// elementdecl         ::=       '<!ELEMENT' S Name S contentspec S? '>'    [VC: Unique Element Type Declaration]
/// contentspec         ::=       'EMPTY' | 'ANY' | Mixed | children
/// Mixed               ::=       '(' S? '#PCDATA' (S? '|' S? Name)* S? ')*' | '(' S? '#PCDATA' S? ')'
/// children            ::=       (choice | seq) [?*+]?
/// choice              ::=       '(' S? cp ( S? '|' S? cp )+ S? ')'    [VC: Proper Group/PE Nesting]
/// seq                 ::=       '(' S? cp ( S? ',' S? cp )* S? ')'    [VC: Proper Group/PE Nesting]
/// cp                  ::=       (Name | choice | seq) [?*+]?
/// 
/// AttlistDecl         ::=       '<!ATTLIST' S Name AttDef* S? '>'
/// AttDef              ::=       S Name S AttType S DefaultDecl
/// AttType             ::=       StringType | TokenizedType | EnumeratedType
/// StringType          ::=       'CDATA'
/// TokenizedType       ::=       'ID' | 'IDREF' | 'IDREFS' | 'ENTITY' | 'ENTITIES' | 'NMTOKEN' | 'NMTOKENS'
/// EnumeratedType      ::=       NotationType | Enumeration
/// NotationType        ::=       'NOTATION' S '(' S? Name (S? '|' S? Name)* S? ')'     [VC: Notation Attributes]
/// Enumeration         ::=       '(' S? Nmtoken (S? '|' S? Nmtoken)* S? ')'    [VC: Enumeration]
/// DefaultDecl         ::=       '#REQUIRED' | '#IMPLIED' | (('#FIXED' S)? AttValue)
/// AttValue            ::=       '"' ([^<&"] | Reference)* '"' |  "'" ([^<&'] | Reference)* "'"
///  
/// EntityDecl          ::=       GEDecl | PEDecl
/// GEDecl              ::=       '<!ENTITY' S Name S EntityDef S? '>'
/// PEDecl              ::=       '<!ENTITY' S '%' S Name S PEDef S? '>'
/// EntityDef           ::=       EntityValue | (ExternalID NDataDecl?)
/// PEDef               ::=       EntityValue | ExternalID
/// NDataDecl           ::=       S 'NDATA' S Name     [VC: Notation Declared]
/// 
/// EntityValue         ::=       '"' ([^%&"] | PEReference | Reference)* '"' |  "'" ([^%&'] | PEReference | Reference)* "'"
/// Reference           ::=       EntityRef | CharRef
/// PEReference         ::=       '%' Name ';'   
/// EntityRef           ::=       '&' Name ';'
/// CharRef             ::=       '&#' [0-9]+ ';' | '&#x' [0-9a-fA-F]+ ';'    [WFC: Legal Character]
/// 
/// NotationDecl        ::=       '<!NOTATION' S Name S (ExternalID | PublicID) S? '>'    [VC: Unique Notation Name]
/// ExternalID          ::=       'SYSTEM' S SystemLiteral | 'PUBLIC' S PubidLiteral S SystemLiteral
/// PublicID            ::=       'PUBLIC' S PubidLiteral
/// SystemLiteral       ::=       ('"' [^"]* '"') | ("'" [^']* "'")
/// PubidLiteral        ::=       '"' PubidChar* '"' | "'" (PubidChar - "'")* "'"
/// PubidChar           ::=       #x20 | #xD | #xA | [a-zA-Z0-9] | [-'()+,./:=?;!*#@$_%]

ExiResult<DoctypeEvent> DTDParser::CreateDTEvent(StrRef Data) {
  using enum SimpleEventTerm;
  Data = Data.trim();
  StrRef Name = TakeToken(Data);
  // Name only
  if (Data.empty()) {
    if EXI_UNLIKELY(Name.empty()) {
      LOG_ERROR("Invalid DOCTYPE! Expected Name.");
      return Err(ErrorCode::kInvalidEXIInput);
    }
    // <!DOCTYPE Name>
    return make_event<DT>(DTK_None, Name);
  } else if (Data.consume_pinch("[", "]"))
    // <!DOCTYPE Name [Data...]>
    return make_event<DT>(DTK_Inline, Name, Data.trim());

  StrRef Kind = TakeToken(Data);
  auto K = StringSwitch<DoctypeKind>(Kind)
    .Case("SYSTEM", DTK_System)
    .Case("PUBLIC", DTK_Public)
    .Default(DTK_None);
  if (K == DTK_None) {
    LOG_ERROR("Invalid DOCTYPE! "
              "Expected SYSTEM or PUBLIC, got '{}'.", Kind);
    return Err(ErrorCode::kInvalidEXIInput);;
  }
  // SYSTEM
  StrRef PrimID = EXI_UNWRAP(
    TakeLiteralToken(Data, K == DTK_System));
  if (K == DTK_System) {
    if (!Data.empty())
      exi_try_r(StripDTText(Data));
    // <!DOCTYPE Name SYSTEM [Data...]?>
    return make_event<DT>(
      DTK_System, Name, PrimID, Data);
  }
  // PUBLIC
  StrRef SysID = EXI_UNWRAP(
    TakeLiteralToken(Data, true));
  if (!SysID.consume_pinch("\"")) {
    LOG_ERROR("Invalid PUBLIC DOCTYPE! "
              "Expected a SystemLiteral, got '{}'.", SysID);
    return Err(ErrorCode::kInvalidEXIInput);;
  }
  if (!Data.empty())
    exi_try_r(StripDTText(Data));
  return make_event<DT>(
    DTK_Public, Name, PrimID, SysID, Data);
}

static usize FindDTSubsectionEnd(StrRef Data, usize Start, const isize Off = 1) {
  isize Count = 0;
  for (isize I = Start + Off, E = Data.size(); I < E; ++I) {
    const char C = Data[I];
    if (C == '>') {
      if (Count == 0)
        // Skip past the closing bracket
        return I + 1;
      --Count;
    }
    // Add to depth
    if (C == '<')
      ++Count;
  }

  return StrRef::npos;
}

static usize FindCMSectionEnd(StrRef Data, usize Start) {
  if EXI_UNLIKELY(Data.size() - Start < 7)
    return StrRef::npos;
  
  // By skipping 6, we can catch invalid sequences like <!-->.
  for (isize I = Start + 4 + 2, E = Data.size(); I < E; ++I) {
    if (Data[I] == '>' && Data[I - 1] == '-' && Data[I - 2] == '-')
      return I + 1;
  }

  return StrRef::npos;
}

static usize FindPISectionEnd(StrRef Data, usize Start) {
  if EXI_UNLIKELY(Data.size() - Start < 4)
    return StrRef::npos;
  
  // By skipping 4, we can catch invalid sequences like <?>.
  for (isize I = Start + 2 + 1, E = Data.size(); I < E; ++I) {
    if (Data[I] == '>' && Data[I - 1] == '?')
      return I + 1;
  }
  return StrRef::npos;
}

template <bool Lazy = false>
static Option<usize> FindDTSectionEnd(StrRef Data, usize Start) {
  exi_invariant(Data[Start] == '<');
  exi_invariant(Start != StrRef::npos);

  if constexpr (Lazy)
    return FindDTSubsectionEnd(Data, Start, /*Offset=*/1);
  else {
    StrRef SData = Data.drop_front(Start + 1);
    // Can be a Comment or a Declaration
    if (SData.consume_front("!")) {
      if (SData.consume_front("--"))
        return FindCMSectionEnd(Data, Start);
      return FindDTSubsectionEnd(Data, Start, /*Offset=*/2);
    }
    // Must be a Processing Instruction
    if (SData.starts_with('?'))
      return FindPISectionEnd(Data, Start);
    // Invalid sequence.
    return std::nullopt;
  }
}

void DTDParser::SplitDTText(StrRef Data, SmallVecImpl<StrRef>& Out) {
  Data = Data.trim(kDelimiter);
  Data.consume_pinch("[", "]");
  while (!Data.empty()) {
    const usize Start = Data.find_first_not_of(kDelimiter);
    if (Start == StrRef::npos)
      return;
    else if EXI_UNLIKELY(Data[Start] != '<') {
      LOG_WARN("Invalid DOCTYPE sequence: '{}'",
               Data.substr(Start, 2));
      return;
    }

    const Option<usize> End = FindDTSectionEnd(Data, Start);
    if EXI_UNLIKELY(!End) {
      LOG_WARN("Invalid DOCTYPE sequence: '{}'",
               Data.substr(Start, 2));
      return;
    } else if EXI_UNLIKELY(*End == StrRef::npos) {
      LOG_WARN("Unterminated DOCTYPE element: {}",
               Data.drop_front(Start));
      return;
    }
    
    Out.emplace_back(Data.slice(Start, *End));
    Data = Data.substr(*End);
  }
}
