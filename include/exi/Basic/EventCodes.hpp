//===- exi/Basic/EventCodes.hpp -------------------------------------===//
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
///
/// \file
/// This file defines an interface for event codes used by the program.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Array.hpp>
#include <core/Common/Fundamental.hpp>
#include <core/Common/EnumArray.hpp>
#include <core/Common/STLExtras.hpp>
#include <core/Common/DenseMapInfo.hpp>

namespace exi {

/// An enum containing all the terminal symbols used for productions.
enum class EventTerm : i32 {
  SD,       // Start Document
  ED,       // End Document
  SE,       // Start Element (*)
  SEUri,    // Start Element (uri:*)
  SEQName,  // Start Element (qname)
  EE,       // End Element
  AT,       // Attribute (*, value)
  ATUri,    // Attribute (uri:*, value)
  ATQName,  // Attribute (qname, value)
  CH,       // Characters (value)
  CHExtern, // Characters (external-value)
  NS,       // Namespace Declaration (uri, prefix, local-element-ns)
  CM,       // Comment text (text)
  PI,       // Processing Instruction (name, text)
  DT,       // DOCTYPE (name, public, system, text)
  ER,       // Entity Reference (name)
  SC,       // Self Contained
  Void,
  Last = SC,
  Invalid = 0b1111111,
};

StrRef get_event_name(EventTerm E) noexcept EXI_READNONE;
StrRef get_event_fullname(EventTerm E) noexcept EXI_READNONE;
StrRef get_event_signature(EventTerm E) noexcept EXI_READNONE;

/// All the data required to output an event code.
/// The data is stored in three u32's.
struct alignas(8) EventCode {
  Array<u32, 3> Data = {}; // [x.y.z]
  Array<u8, 3> Bits = {};  // [[x].[y].[z]]
  i8 Length = 0;           // Number of pieces.
};

static_assert(sizeof(EventCode) == sizeof(u32) * 4);
static_assert(alignof(EventCode) >= 8);

//===----------------------------------------------------------------===//
// Small QName
//===----------------------------------------------------------------===//

enum : u64 {
  /// Invalid URI for `SmallQName`.
  kInvalidURI     = 0xFFFF,
  /// Invalid LocalName for `SmallQName`.
  kInvalidLNI     = 0xFFFFFFFFFFFF,
  /// Invalid LocalName for `EventUID`.
  kInvalidPrefix  = 0xFF,
  /// Invalid Terminal for `EventUID`.
  kInvalidTerm    = u64(EventTerm::Invalid),
  /// Invalid Value for `EventUID`.
  kInvalidVID     = 0xFFFFFFFFFFFF,
};

/// A compressed version of a QName, only represents IDs.
struct SmallQName {
  /// The LocalName for the current QName.
  u64 LocalID : 48 = kInvalidLNI;
  /// The URI for the current QName.
  u64 URI : 16 = kInvalidURI;

public:
  /// Creates a SmallQName with a `(*)` value.
  ALWAYS_INLINE static constexpr SmallQName NewAny() {
    return {};
  }
  /// Creates a SmallQName with a `(uri:*)` value.
  ALWAYS_INLINE static constexpr SmallQName NewURI(u64 URI) {
    return {.URI = URI};
  }
  /// Creates a SmallQName with a `(uri:name)` value.
  ALWAYS_INLINE static constexpr SmallQName NewQName(u64 URI, u64 LocalName) {
    // TODO: Add checks?
    exi_invariant(URI < kInvalidURI && LocalName < kInvalidLNI);
    return {.LocalID = LocalName, .URI = URI};
  }

  /// Checks if QName has a `(uri:?)`.
  constexpr bool hasURI() const {
    return URI != kInvalidURI;
  }
  /// Checks if QName has a `(?:name)`.
  constexpr bool hasLocalName() const {
    return URI != kInvalidLNI;
  }
  /// Checks if QName has a `(?:name)`.
  constexpr bool hasName() const {
    return URI != kInvalidLNI;
  }

  /// Checks if QName has a `(*)` value.
  constexpr bool isAny() const {
    return !hasURI() && !hasName();
  }
  /// Checks if QName has a `(uri:*)` value.
  constexpr bool isUri() const {
    return hasURI() && !hasName();
  }
  /// Checks if QName has a `(uri:name)` value.
  constexpr bool isQName() const {
    return hasURI() && hasName();
  }

  EXI_INLINE constexpr bool operator==(const SmallQName& RHS) const = default;
};

// Provide DenseMapInfo for `SmallQName`.
template <> struct DenseMapInfo<SmallQName> {
  /// QNames of type `(*)` are not "real" QNames, at least to the user. They
  /// just inform the parser it must decode the real value, as it is unknown.
  /// Because of this, we can use it as the empty key.
  static inline SmallQName getEmptyKey() { return {}; }

  /// QNames of type `(*:name)` are invalid. They can be used for tombstones.
  static inline SmallQName getTombstoneKey() { return {.URI = 0}; }

  static unsigned getHashValue(const SmallQName& Val) {
    return DenseMapInfo<u64>::getHashValue(Get(Val));
  }

  static bool isEqual(const SmallQName& LHS, const SmallQName& RHS) {
    return Get(LHS) == Get(RHS);
  }

private:
  ALWAYS_INLINE static u64 Get(const SmallQName& Val) {
    return exi::bit_cast<u64>(Val);
  }
};

//===----------------------------------------------------------------===//
// Unique ID
//===----------------------------------------------------------------===//

/// A compressed Unique IDentifier for event codes. Allows for simpler lookup.
struct EventUID {
  /// The Prefix for the current QName (if applicable).
  u64 ValueID : 48 = kInvalidVID;

  /// The Prefix for the current QName (if applicable).
  u64 Prefix : 8 = kInvalidPrefix;
  
  EXI_PREFER_TYPE(EventTerm)
  /// The Terminal for the current event.
  u64 Term : 7 = kInvalidTerm;
  
  EXI_PREFER_TYPE(bool)
  /// If a Value is local or global.
  u64 IsLocal : 1 = false;
  
  /// Optional QName data, only representing a `(uri:name)` value.
  SmallQName Name = {};

public:
  /// Creates a new null UID.
  static constexpr EventUID NewNull() { return {}; }

  /// Creates a new UID with only a term.
  static constexpr EventUID NewTerm(EventTerm Term) {
    return { .Term = static_cast<u64>(Term) };
  }

  /// Creates a new unbound QName.
  static constexpr EventUID NewQName(
   SmallQName Name, Option<u64> Pfx = std::nullopt) {
    const u64 RealPfx = Pfx.value_or(kInvalidPrefix);
    return { .Prefix = RealPfx, .Name = Name };
  }

  /// Creates a new unbound Namespace.
  static constexpr EventUID NewNS(
   SmallQName Name, u64 Pfx, bool IsLocal = false) {
    return { .Prefix = Pfx, .IsLocal = IsLocal, .Name = Name };
  }

  /// Creates a new unbound GlobalValue.
  static constexpr EventUID NewGlobalValue(u64 ID) {
    return {.ValueID = ID, .IsLocal = false};
  }

  /// Creates a new unbound GlobalValue.
  static constexpr EventUID NewLocalValue(SmallQName Name, u64 ID) {
    return {.ValueID = ID, .IsLocal = true, .Name = Name};
  }

  /// Checks if Prefix is active.
  constexpr bool hasTerm() const {
    return Term != kInvalidTerm;
  }
  /// Checks if Name has a `(uri:?)`.
  constexpr bool hasURI() const {
    return Name.hasURI();
  }
  /// Checks if Name has a `(?:name)`.
  constexpr bool hasName() const {
    return Name.hasLocalName();
  }
  /// Checks if Name has a `(uri:name)`.
  constexpr bool hasQName() const {
    return Name.isQName();
  }
  /// Checks if Prefix is active.
  constexpr bool hasPrefix() const {
    return Prefix != kInvalidPrefix;
  }
  /// Checks if Prefix is active.
  constexpr bool hasValue() const {
    return ValueID != kInvalidVID;
  }

  /// Checks if Prefix is active.
  constexpr bool isGlobal() const { return !IsLocal; }
  /// Checks if Prefix is active.
  constexpr bool isLocal() const { return IsLocal; }

  /// Gets a term as an `EventTerm`.
  static constexpr EventTerm GetTerm(u64 Term) EXI_READNONE {
    constexpr_static u64 VoidTerm = u64(EventTerm::Void);
    if EXI_UNLIKELY(Term > VoidTerm)
      return EventTerm::Void;
    return static_cast<EventTerm>(Term);
  }

  /// Gets the current term as an `EventTerm`.
  ALWAYS_INLINE constexpr EventTerm getTerm() const {
    return EventUID::GetTerm(Term);
  }
  /// Gets the current URI.
  constexpr u64 getURI() const {
    exi_invariant(this->hasURI());
    return Name.URI;
  }
  /// Gets the current LocalName.
  constexpr u64 getName() const {
    exi_invariant(this->hasName());
    return Name.LocalID;
  }
  /// Gets the current Prefix.
  constexpr u64 getPrefix() const {
    exi_invariant(this->hasPrefix());
    return Prefix;
  }
  /// Gets the current Value ID.
  constexpr u64 getValue() const {
    exi_invariant(this->hasValue());
    return ValueID;
  }

  constexpr void setTerm(EventTerm Term) {
    this->Term = static_cast<u64>(Term);
  }

  constexpr operator EventTerm() const {
    return EventUID::GetTerm(Term);
  }

  explicit constexpr operator bool() const {
    return hasTerm() || hasName() || hasValue();
  }
};

} // namespace exi
