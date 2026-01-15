//===- exi/Encode/DTDParser.hpp --------------------------------------===//
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

#pragma once

#include <core/Common/StringSwitch.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Common/Unwrap.hpp>
#include <core/Common/bitset.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Encode/Event.hpp>

#define DEBUG_TYPE "DTDParser"

namespace exi {

/// Implements DOCTYPE parsing functions.
struct DTDParser {
  /// Delimiters used by DTDs.
  static constexpr StrRef kDelimiter = " \t\n\r"_str;

  /// Consumes a token and advances to the next (or the end).
  [[nodiscard]] static StrRef TakeToken(StrRef& S) {
    const auto Pos = S.find_first_of(kDelimiter);
    if (Pos == StrRef::npos) {
      StrRef Out = S;
      S = "";
      return Out;
    }
    StrRef Out = S.take_front(Pos);
    S = S.drop_front(Pos).ltrim(kDelimiter);
    return Out;
  }

  /// Per the XML EBNF:
  ///  `#x20 | #xD | #xA | [a-zA-Z0-9] | [-'()+,./:=?;!*#@$_%]`
  static consteval StrRef::filter_t GetPubidCharFilter() {
    StrRef::filter_t Filter {};
    // #x20 | #xD | #xA
    Filter.set(' ').set('\r').set('\n');
    // [a-zA-Z0-9]
    Filter.set_range('A','Z')
          .set_range('a','z')
          .set_range('0','9');
    // [-'()+,./:=?;!*#@$_%]
    for (unsigned char C : "-'()+,./:=?;!*#@$_%")
      Filter.set(C);
    return Filter;
  }

  /// Per the XML EBNF:
  ///  `" PubidChar* " | ' (PubidChar - ')* '`
  static StrRef::filter_t GetPubidLiteralFilter(char Quote) {
    const bool kAllowQuote = (Quote == '\"');
    auto Filter = GetPubidCharFilter();
    return Filter.set('\'', kAllowQuote);
  }

  /// Takes tokens with the format `"..." | '...'`.
  static ExiResult<StrRef> TakeLiteralToken(StrRef& S, bool IsSystem = true) {
    static constexpr StrRef kLiteralName[] = {"PubidLiteral", "SystemLiteral"};
    if EXI_UNLIKELY(S.size() < 2 || (S[0] != '\"' && S[0] != '\'')) {
      LOG_ERROR("Invalid DOCTYPE! Expected a {}, got '{}'.",
                kLiteralName[IsSystem], S);
      return Err(ErrorCode::kInvalidEXIInput);
    }

    const char Quote = S[0];
    const auto Pos = S.find_first_of(Quote, 1);
    if EXI_UNLIKELY(Pos == StrRef::npos) {
      LOG_ERROR("Invalid DOCTYPE! Unterminated {} in {}: `{}`.",
                Quote, kLiteralName[IsSystem], S);
      return Err(ErrorCode::kInvalidEXIInput);
    }

    StrRef Out = S.take_front(Pos).drop_front();
#if !EXI_PERMISSIVE
    if (!IsSystem && !Out.empty()) {
      const auto Filter = GetPubidLiteralFilter(Quote);
      for (unsigned char C : Out) {
        if EXI_UNLIKELY(C > 127 || !Filter.test(C)) {
          LOG_WARN("Invalid DOCTYPE! "
                   "Invalid characters in PubidLiteral: {}.",
                   S.take_front(Pos + 1));
          // TODO: Allow "invalid" characters in different mode?
          return Err(ErrorCode::kInvalidEXIInput);
        }
      }
    }
#endif
    S = S.drop_front(Pos + 1).ltrim(" \t\n\r");
    return Out;
  }

  static ExiError StripDTText(StrRef& S) {
    if EXI_LIKELY(S.consume_pinch("[", "]")) {
      S = S.trim();
      return ExiError::OK;
    }
    LOG_ERROR("Invalid DOCTYPE! Expected [<text>], got '{}'", S);
    return ErrorCode::kInvalidEXIInput;
  }

  [[nodiscard]] static ExiResult<DoctypeEvent> CreateDTEvent(StrRef Data) {
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
};

} // namespace exi

#undef DEBUG_TYPE
