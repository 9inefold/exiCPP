//===- exi/Encode/Event.hpp ------------------------------------------===//
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

#pragma once

#include <core/Common/Fundamental.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Common/VariadicFunction.hpp>
#include <exi/Basic/EventTerms.hpp>
#include <exi/Encode/D/EventMappings.mac>
#include <exi/Encode/StringTableHandles.hpp>
#include <concepts>

namespace exi {

class BodyEncoder;
class ExiError;
class raw_ostream;

namespace encode {
class Schema;
} // namespace encode

enum class DoctypeKind : u8 {
  DTK_None   = 1, // Name
  DTK_Text   = 2, // Name Text
  DTK_System = 3, // Name SYSTEM "sysid" Text?
  DTK_Public = 4, // Name PUBLIC "pubid" "sysid" Text?
};

using enum DoctypeKind;
StrRef get_doctype_name(DoctypeKind K) EXI_READNONE;

/// Marks all events as such.
struct alignas(8) BaseEvent {};

/// The base of all string types.
struct EXI_EMPTY_BASES NoEventData : BaseEvent {};
/// The base of all string types.
struct EXI_EMPTY_BASES StringEventData : BaseEvent {
  u32 Size = 0;
  u32 Extra : 32 = 0;
  u32 Tag : 2 = 0b00;
  const char* Data = nullptr;
};
/// The base of all (string, string) types.
struct EXI_EMPTY_BASES PairEventData : BaseEvent {
  u32 Size[2] = {0, 0};
  const char* Data[2] = {nullptr, nullptr};
public:
  constexpr ALWAYS_INLINE StrRef operator[](usize Ix) const 
   EXI_ERROR_IF(Ix > 1, "Index out of range!") {
    exi_invariant(Ix <= 1, "Index out of range!");
    return StrRef(Data[Ix], Size[Ix]);
  }
};

// Start Element (qname)
struct StartElemEvent : StringEventData {};
// Start Element (uri:*)
struct StartElemURIEvent : StartElemEvent {
  union {
    const char* URI;
    encode::STURIEntry* OpaqueURI;
  };
};
/// End Element
struct EndElemEvent   : NoEventData {};
/// Attribute (string, string)
struct AttrEvent      : PairEventData {};
/// Characters (string)
struct CharEvent      : StringEventData {};
/// Namespace (string, string, bool)
struct NamespaceEvent : BaseEvent {
  u32 PfxSize = 0;
  u32 UriSize : 31 = 0;
  u32 IsLocal : 1  = false;
  const char* PfxData = nullptr;
  const char* UriData = nullptr;
};
/// Start Document
struct StartDocEvent  : NoEventData {};
/// End Document
struct EndDocEvent    : NoEventData {};
/// Comment (string)
struct CommentEvent   : StringEventData {};
/// Processing Instruction (string, string)
struct ProcInstrEvent : PairEventData {};
/// DOCTYPE (string, string*, u8kind)
struct DoctypeEvent   : BaseEvent {
  DoctypeKind Kind = DTK_Text;
  u32 Size[4] = {};
  const char* Data[4] = {};
public:
  constexpr ALWAYS_INLINE StrRef operator[](usize Ix) const 
   EXI_ERROR_IF(Ix > 3, "Index out of range!") {
    exi_invariant(Ix <= 3, "Index out of range!");
    return StrRef(Data[Ix], Size[Ix]);
  }
};
/// Entity Reference (string)
struct EntityRefEvent : StringEventData {};
/// Self Contained
struct SelfContEvent  : NoEventData {};

template <typename T>
concept is_encode_event = std::derived_from<
  std::remove_cvref_t<T>, BaseEvent>;

template <typename T>
concept is_empty_event = std::derived_from<
  std::remove_cvref_t<T>, NoEventData>;

template <typename T>
concept is_string_event = std::derived_from<
  std::remove_cvref_t<T>, StringEventData>;

template <typename T>
concept is_stringpair_event = std::derived_from<
  std::remove_cvref_t<T>, PairEventData>;

//////////////////////////////////////////////////////////////////////////
// Mappings

namespace H {

template <SimpleEventTerm Term> struct MapTermToEvent {
  COMPILE_FAILURE(Term, "Invalid SimpleEventTerm!");
};
template <is_encode_event Event> struct UnmapTermToEvent {
  COMPILE_FAILURE(Event, "Invalid Event!");
};
template <> struct UnmapTermToEvent<StartElemURIEvent> {
  static constexpr SimpleEventTerm value = SimpleEventTerm::SE;
};

#define MAP_TERM(FROM, TO)                                                    \
template <> struct MapTermToEvent<SimpleEventTerm::FROM>                      \
{ using type = TO; };                                                         \
template <> struct UnmapTermToEvent<TO>                                       \
{ static constexpr SimpleEventTerm value = SimpleEventTerm::FROM; };

EXI_ENCODE_EVENT_MAPPINGS(MAP_TERM)

#undef MAP_TERM

DoctypeEvent MakeDTEventImpl(DoctypeKind Kind,
                             ArrayRef<const StrRef*> Args);
ExiError EncodeDTWithImpl(encode::Schema* S, BodyEncoder* BE,
                          DoctypeKind Kind, ArrayRef<const StrRef*> Args);

} // namespace H

/// Maps a `SimpleEventTerm` to the corresponding `*Event`.
template <SimpleEventTerm Term>
using map_event_t = typename H::MapTermToEvent<Term>::type;

/// Unmaps a `*Event` to the corresponding `SimpleEventTerm`.
template <is_encode_event Term>
inline constexpr SimpleEventTerm unmap_event_v
  = H::UnmapTermToEvent<std::remove_cvref_t<Term>>::value;

//////////////////////////////////////////////////////////////////////////
// make/cast

template <SimpleEventTerm Term> struct MakeEvent {
  using Type = map_event_t<Term>;
  static_assert(std::is_empty_v<Type>);
  ALWAYS_INLINE constexpr Type operator()() const {
    return Type{{}};
  }
};

template <SimpleEventTerm Term>
requires is_string_event<map_event_t<Term>>
struct MakeEvent<Term> {
  using Type = map_event_t<Term>;
  inline constexpr Type operator()(StrRef Data) const {
    return Type{{
      .Size = unsigned(Data.size()),
      .Data = Data.data()
    }};
  }
};

template <SimpleEventTerm Term>
requires is_stringpair_event<map_event_t<Term>>
struct MakeEvent<Term> {
  using Type = map_event_t<Term>;
  inline constexpr Type operator()(StrRef Name, StrRef Data) const {
    return Type{{
      .Size = {unsigned(Name.size()), unsigned(Data.size())},
      .Data = {Name.data(), Data.data()}
    }};
  }
};

template <> struct MakeEvent<SimpleEventTerm::SE> {
  using Type = StartElemEvent;
  inline constexpr Type operator()(StrRef Data) const {
    return Type{{
      .Size = unsigned(Data.size()),
      .Data = Data.data()
    }};
  }
  using TypeEx = StartElemURIEvent;
  inline constexpr TypeEx operator()(StrRef Data, StrRef URI) const {
    return TypeEx{{{
      .Size = unsigned(Data.size()),
      .Extra = unsigned(URI.size()), .Tag = 1,
      .Data = Data.data()
    }}, {
      .URI = URI.data()
    }};
  }
  inline constexpr TypeEx operator()(StrRef Data,
                                     encode::STURIEntry* URI) const {
    return TypeEx{{{
      .Size = unsigned(Data.size()),
      .Tag  = 2,
      .Data = Data.data()
    }}, {
      .OpaqueURI = URI
    }};
  }
};

template <> struct MakeEvent<SimpleEventTerm::NS> {
  using Type = NamespaceEvent;
  inline constexpr Type operator()(StrRef Pfx, StrRef Uri,
                                   bool IsLocal = false) const {
    return Type{
      .PfxSize = unsigned(Pfx.size()),
      .UriSize = unsigned(Uri.size()),
      .IsLocal = IsLocal,
      .PfxData = Pfx.data(),
      .UriData = Uri.data(),
    };
  }
};

template <> struct MakeEvent<SimpleEventTerm::DT>
 : public variadic_function<&H::MakeDTEventImpl, 4> {
  using Type = DoctypeEvent;
  using variadic_function<&H::MakeDTEventImpl, 4>::operator();
  DoctypeEvent operator()(DoctypeKind K, ArrayRef<StrRef> Args);
};

template <SimpleEventTerm Term>
inline constexpr MakeEvent<Term> make_event;

template <SimpleEventTerm Term, class Event = map_event_t<Term>>
ALWAYS_INLINE constexpr Event& event_cast(BaseEvent& BE) {
  return static_cast<Event&>(BE);
}

template <SimpleEventTerm Term, class Event = map_event_t<Term>>
ALWAYS_INLINE constexpr const Event& event_cast(const BaseEvent& BE) {
  return static_cast<const Event&>(BE);
}

template <SimpleEventTerm Term, class Event = map_event_t<Term>>
EXI_INLINE constexpr Event& event_cast(
 BaseEvent& BE, [[maybe_unused]] SimpleEventTerm K) {
  exi_invariant(K == Term, "Invalid term_cast!");
  return event_cast<Term, Event>(BE);
}

template <SimpleEventTerm Term, class Event = map_event_t<Term>>
EXI_INLINE constexpr const Event& event_cast(
 const BaseEvent& BE, [[maybe_unused]] SimpleEventTerm K) {
  exi_invariant(K == Term, "Invalid term_cast!");
  return event_cast<Term, Event>(BE);
}

template <is_encode_event Event>
constexpr StrRef get_event_name(const Event&) {
  return get_event_name(unmap_event_v<Event>);
}

//inline constexpr variadic_function<&H::EncodeDTWithImpl, 4> EncodeDTWith;

//////////////////////////////////////////////////////////////////////////
// raw_ostream

inline raw_ostream& operator<<(raw_ostream& OS, const NoEventData&) {return OS;}
raw_ostream& operator<<(raw_ostream& OS, const StringEventData& Event);
raw_ostream& operator<<(raw_ostream& OS, const PairEventData& Event);

raw_ostream& operator<<(raw_ostream& OS, const StartElemEvent& SE);
raw_ostream& operator<<(raw_ostream& OS, const NamespaceEvent& NS);
raw_ostream& operator<<(raw_ostream& OS, const DoctypeEvent& DT);

} // namespace exi
