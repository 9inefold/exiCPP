//===- exi/Encode/Event.cpp ------------------------------------------===//
//
// Copyright (C) 2025-2026 Ninefold
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
/// This file implements the value forms of EXI events.
///
//===----------------------------------------------------------------===//

#include <exi/Encode/Event.hpp>
#include <core/Common/STLExtras.hpp>
#include <core/Common/StringExtras.hpp>
#include <core/Support/IntCast.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/Except.hpp>
#include <exi/Encode/StringTable.hpp>
#include <exi/Grammar/EncoderSchema.hpp>

using namespace exi;
using namespace exi::encode;

#define DEBUG_TYPE "DoctypeEvent"

GCC_IGNORED("-Wmissing-field-initializers")

static_assert(sizeof(StringEventData) == 2 * sizeof(void*));
static_assert(sizeof(StringEventData) == sizeof(StartElemEvent));
static_assert(sizeof(StartElemURIEvent) == 3 * sizeof(void*));
static_assert(sizeof(NamespaceEvent) == 3 * sizeof(void*));

StrRef exi::get_doctype_name(DoctypeKind K) {
  switch (K) {
  case DTK_None:   return "";
  case DTK_Text:   return "Text";
  case DTK_System: return "SYSTEM";
  case DTK_Public: return "PUBLIC";
  }
  exi_guardrail("invalid DOCTYPE type");
}

//static void PushBackSingleArgument(SmallStr<kSmallStrLen>& S, StrRef Arg) {
//  while (!Arg.empty()) {
//    usize Off = Arg.find_first_of('\n');
//    S.append(Arg.take_front(Off));
//    if (Off == StrRef::npos)
//      return;
//    Arg = Arg.drop_front(Off + 1);
//  }
//}

template <typename T>
NO_INLINE static void WarnOnInvalidDTArgs(DoctypeKind Kind, ArrayRef<T> Args) {
  SmallStr<80> Buf;
  raw_svector_ostream OS(Buf);

  ListSeparator LS;
  for (auto Arg : Args) {
    if constexpr (std::is_pointer_v<T>) {
      if EXI_LIKELY(Arg)
        exi::printCStyleEscapedString(*Arg, OS << LS);
    } else
      exi::printCStyleEscapedString(Arg, OS << LS);
  }
  
  LOG_WARN("Too many arguments to make_event<DT>({}, ...)! "
           "Expected <= {}, got {}: {{{}}}",
           get_doctype_name(Kind),
           unsigned(Kind), Args.size(), Buf.str()
  );
}

template <typename T>
ALWAYS_INLINE static void ValidateDTArgs(DoctypeKind Kind, ArrayRef<T>& Args) {
  const unsigned kMaxArgs = unsigned(Kind);
  if EXI_LIKELY(Args.size() <= kMaxArgs)
    return;
  
  WarnOnInvalidDTArgs(Kind, Args);
  Args = Args.take_front(kMaxArgs);
#if !EXI_PERMISSIVE
  Throw<argument_error>("Too many arguments passed to make_event<DT>(...)!");
#endif
}

DoctypeEvent exi::H::MakeDTEventImpl(DoctypeKind K,
                                     ArrayRef<const StrRef*> Args) {
  ValidateDTArgs(K, Args);
  DoctypeEvent Out {.Kind = K};
  for (auto [Ix, Arg] : exi::enumerate(Args)) {
    Out.Size[Ix] = IntCast<unsigned>(Arg->size());
    Out.Data[Ix] = Arg->data();
  }
  return Out;
}

ExiError exi::H::EncodeDTWithImpl(encode::Schema* S,
                                  BodyEncoder* BE, DoctypeKind Kind,
                                  ArrayRef<const StrRef*> Args) {
  DoctypeEvent DT = H::MakeDTEventImpl(Kind, Args);
  return S->encode(BE, DT);
}

DoctypeEvent MakeEvent<SimpleEventTerm::DT>::operator()(DoctypeKind K,
                                                        ArrayRef<StrRef> Args) {
  ValidateDTArgs(K, Args);
  Type Out {.Kind = K};
  for (auto [Ix, Arg] : exi::enumerate(Args)) {
    Out.Size[Ix] = IntCast<unsigned>(Arg.size());
    Out.Data[Ix] = Arg.data();
  }
  return Out;
}

//////////////////////////////////////////////////////////////////////////
// operator==

template <bool Opaque>
static StrRef GetSEURI(const StartElemEvent& SE) {
  static constexpr StringEventKind TAG
    = Opaque ? StringEventKind::Opaque
             : StringEventKind::URI;
  exi_invariant(SE.tag() == TAG, "Incorrect type!");
  auto* SEUri = &static_cast<const StartElemURIEvent&>(SE);
  if constexpr (!Opaque)
    return StrRef(SEUri->URI, SEUri->Extra);
  else
    return StringTable::GetURI(SEUri->OpaqueURI);
}

static StrRef RuntimeGetSEURI(const StartElemEvent& SE) {
  auto* SEUri = &static_cast<const StartElemURIEvent&>(SE);
  switch (SE.tag()) {
  case StringEventKind::Simple:
    return ""_str; // TODO: Decide if this should be nullopt?
  case StringEventKind::URI:
    return StrRef(SEUri->URI, SEUri->Extra);
  case StringEventKind::Opaque:
    return StringTable::GetURI(SEUri->OpaqueURI);
  default:
    exi_guardrail("invalid SE tag type");
  }
}

// TODO: TEST THIS!!! IT DESPERATELY NEEDS TESTING
static bool CompareIncompatibleTags(const StartElemEvent& LHS,
                                    const StartElemEvent& RHS) {
  exi_invariant(LHS.name() == RHS.name());
  static_assert(u32(StringEventKind::Simple) == 1, "Tags out of sync!");
  if (LHS.Tag <= 1 || RHS.Tag <= 1)
    // TODO: Check if this assumption is true.
    // It should be since it should only appear the first time... but I'm kinda
    // tired rn so I could be wrong lol
    return true;
  else
    return RuntimeGetSEURI(LHS) == RuntimeGetSEURI(RHS);
}

bool exi::operator==(const StartElemEvent& LHS, const StartElemEvent& RHS) {
  exi_assert(LHS.Tag != 0 && RHS.Tag != 0);
  if (LHS.name() != RHS.name())
    return false;
  if (LHS.tag() != RHS.tag())
    return CompareIncompatibleTags(LHS, RHS);
  // SE(qname)
  if (LHS.tag() == StringEventKind::Simple)
    return true;
  // SE(uri:*)
  else if (LHS.tag() == StringEventKind::URI)
    return GetSEURI<false>(LHS) == GetSEURI<false>(RHS);
  else if (LHS.tag() == StringEventKind::Opaque)
    return GetSEURI<true>(LHS) == GetSEURI<true>(RHS);
  exi_guardrail("invalid SE tag type");
}

bool DoctypeEvent::equals(const DoctypeEvent& RHS) const {
  exi_todo("Implement equals");
}

bool DoctypeEvent::equalsEx(const DoctypeEvent& RHS) const {
  exi_todo("Implement equalsEx");
}

bool exi::operator==(const DoctypeEvent& LHS, const DoctypeEvent& RHS) {
  if (LHS.Kind != RHS.Kind)
    return false;
  
  switch (LHS.Kind) {
  case DTK_Public:
    if (LHS[3] != RHS[3])
      return false;
    [[fallthrough]];
  case DTK_System:
    if (LHS[2] != RHS[2])
      return false;
    [[fallthrough]];
  case DTK_Inline:
    if (LHS[1] != RHS[1])
      return false;
    [[fallthrough]];
  case DTK_None:
    return LHS[0] == RHS[0];
  }

  exi_guardrail("Invalid DTD type!");
}

//////////////////////////////////////////////////////////////////////////
// raw_ostream

raw_ostream& exi::operator<<(raw_ostream& OS, const StringEventData& Event) {
  return OS << format("'{}'", StrRef(Event.Data, Event.Size));
}

raw_ostream& exi::operator<<(raw_ostream& OS, const PairEventData& Event) {
  return OS << format("'{}' -> {}", Event[0], Event[1]);
}

raw_ostream& exi::operator<<(raw_ostream& OS, const StartElemEvent& SE) {
  StrRef Name(SE.Data, SE.Size);
  switch (SE.Tag) {
  case 0:
    return OS << Name;
  case 1:
    return OS << Name << '=' << GetSEURI<false>(SE);
  case 2:
    return OS << Name << '=' << GetSEURI<true>(SE);
  default:
    return OS << "INVALID-TAG";
  }
}

raw_ostream& exi::operator<<(raw_ostream& OS, const NamespaceEvent& NS) {
  return OS << format("{}=\"{}\"{}",
    StrRef(NS.PfxData, NS.PfxSize),
    StrRef(NS.UriData, NS.UriSize),
    (NS.IsLocal ? " (local)" : ""));
}

raw_ostream& exi::operator<<(raw_ostream& OS, const DoctypeEvent& DT) {
  switch (DT.Kind) {
  case DTK_None:
    return OS << format("{}", DT[0]);
  case DTK_Text:
    return OS << format("{} [{}]", DT[0], DT[1]);
  // TODO: When quoting, check for " and switch to ' if necessary
  case DTK_System:
    return OS << format("{} SYSTEM \"{}\" [{}]", DT[0], DT[1], DT[2]);
  case DTK_Public:
    return OS << format("{} PUBLIC \"{}\" \"{}\" [{}]", DT[0], DT[1], DT[2], DT[3]);
  }
  exi_guardrail("invalid DOCTYPE type.");
}
