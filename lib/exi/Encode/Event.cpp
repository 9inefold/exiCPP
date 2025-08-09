//===- exi/Encode/Event.cpp ------------------------------------------===//
//
// Copyright (C) 2025 Eightfold
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
#include <core/Support/IntCast.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/Except.hpp>
#include <exi/Grammar/EncoderSchema.hpp>

using namespace exi;

#define DEBUG_TYPE "DoctypeEvent"

StrRef exi::get_doctype_name(DoctypeKind K) {
  switch (K) {
  case DTK_None:   return "";
  case DTK_Text:   return "Text";
  case DTK_System: return "SYSTEM";
  case DTK_Public: return "PUBLIC";
  }
}

template <typename T>
ALWAYS_INLINE static void ValidateDTArgs(DoctypeKind Kind, ArrayRef<T>& Args) {
  const unsigned kMaxArgs = unsigned(Kind);
  if EXI_LIKELY(Args.size() <= kMaxArgs)
    return;
  
  LOG_WARN("Too many arguments to make_event<DT>({}, ...)! "
           "Expected <= {}, got {}.",
           get_doctype_name(Kind),
           kMaxArgs, Args.size()
  );
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
// raw_ostream

raw_ostream& exi::operator<<(raw_ostream& OS, const StringEventData& Event) {
  return OS << format("'{}'", StrRef(Event.Data, Event.Size));
}

raw_ostream& exi::operator<<(raw_ostream& OS, const PairEventData& Event) {
  return OS << format("'{}' -> {}", Event[0], Event[1]);
}

raw_ostream& exi::operator<<(raw_ostream& OS, const NamespaceEvent& NS) {
  return OS << format("{}=\"{}\"{}",
    NS[0], NS[1], (NS.IsLocal ? " (local)" : ""));
}

raw_ostream& exi::operator<<(raw_ostream& OS, const DoctypeEvent& DT) {
  switch (DT.Kind) {
  case DTK_None:
    return OS << format("{}", DT[0]);
  case DTK_Text:
    return OS << format("{} [{}]", DT[0], DT[1]);
  case DTK_System:
    return OS << format("{} SYSTEM \"{}\" [{}]", DT[0], DT[1], DT[2]);
  case DTK_Public:
    return OS << format("{} PUBLIC \"{}\" \"{}\" [{}]", DT[0], DT[1], DT[2], DT[3]);
  }
}
