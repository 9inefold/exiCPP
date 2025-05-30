//===- Common/Twine.hpp ---------------------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Relicensed under the Apache License, Version 2.0 (the "License");
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
/// This file defines the Twine class for fast string concatenation.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <Common/SmallVec.hpp>
#include <Common/String.hpp>
#include <Support/ErrorHandle.hpp>

namespace exi {

class IFormatObject;
class raw_ostream;

/// Twine - A lightweight data structure for efficiently representing the
/// concatenation of temporary values as strings.
///
/// A Twine is a kind of rope, it represents a concatenated string using a
/// binary-tree, where the string is the preorder of the nodes. Since the
/// Twine can be efficiently rendered into a buffer when its result is used,
/// it avoids the cost of generating temporary values for intermediate string
/// results -- particularly in cases when the Twine result is never
/// required. By explicitly tracking the type of leaf nodes, we can also avoid
/// the creation of temporary strings for conversions operations (such as
/// appending an integer to a string).
///
/// A Twine is not intended for use directly and should not be stored, its
/// implementation relies on the ability to store pointers to temporary stack
/// objects which may be deallocated at the end of a statement. Twines should
/// only be used as const references in arguments, when an API wishes
/// to accept possibly-concatenated strings.
///
/// Twines support a special 'null' value, which always concatenates to form
/// itself, and renders as an empty string. This can be returned from APIs to
/// effectively nullify any concatenations performed on the result.
///
/// \b Implementation
///
/// Given the nature of a Twine, it is not possible for the Twine's
/// concatenation method to construct interior nodes; the result must be
/// represented inside the returned value. For this reason a Twine object
/// actually holds two values, the left- and right-hand sides of a
/// concatenation. We also have nullary Twine objects, which are effectively
/// sentinel values that represent empty strings.
///
/// Thus, a Twine can effectively have zero, one, or two children. The \see
/// isNullary(), \see isUnary(), and \see isBinary() predicates exist for
/// testing the number of children.
///
/// We maintain a number of invariants on Twine objects (FIXME: Why):
///  - Nullary twines are always represented with their Kind on the left-hand
///    side, and the Empty kind on the right-hand side.
///  - Unary twines are always represented with the value on the left-hand
///    side, and the Empty kind on the right-hand side.
///  - If a Twine has another Twine as a child, that child should always be
///    binary (otherwise it could have been folded into the parent).
///
/// These invariants are check by \see isValid().
///
/// \b Efficiency Considerations
///
/// The Twine is designed to yield efficient and small code for common
/// situations. For this reason, the concat() method is inlined so that
/// concatenations of leaf nodes can be optimized into stores directly into a
/// single stack allocated object.
///
/// In practice, not all compilers can be trusted to optimize concat() fully,
/// so we provide two additional methods (and accompanying operator+
/// overloads) to guarantee that particularly important cases (cstring plus
/// StrRef) codegen as desired.
class Twine {
  /// NodeKind - Represent the type of an argument.
  enum NodeKind : unsigned char {
    /// An empty string; the result of concatenating anything with it is also
    /// empty.
    NullKind,

    /// The empty string.
    EmptyKind,

    /// A pointer to a Twine instance.
    TwineKind,

    /// A pointer to a C string instance.
    CStringKind,

    /// A pointer to an String instance.
    StdStringKind,

    /// A Pointer and Length representation. Used for std::string_view,
    /// StrRef, and SmallStr.  Can't use a StrRef here
    /// because they are not trivally constructible.
    PtrAndLengthKind,

    /// A pointer and length representation that's also null-terminated.
    /// Guaranteed to be constructed from a compile-time string literal.
    StringLiteralKind,

    /// A pointer to an IFormatObject instance.
    FormatObjectKind,

    /// A char value, to render as a character.
    CharKind,

    /// An unsigned int value, to render as an unsigned decimal integer.
    DecUIKind,

    /// An int value, to render as a signed decimal integer.
    DecIKind,

    /// A pointer to an unsigned long value, to render as an unsigned decimal
    /// integer.
    DecULKind,

    /// A pointer to a long value, to render as a signed decimal integer.
    DecLKind,

    /// A pointer to an unsigned long long value, to render as an unsigned
    /// decimal integer.
    DecULLKind,

    /// A pointer to a long long value, to render as a signed decimal integer.
    DecLLKind,

    /// A pointer to a u64 value, to render as an unsigned hexadecimal
    /// integer.
    UHexKind
  };

  union Child
  {
    const Twine *twine;
    const char *cString;
    const String *stdString;
    struct {
      const char *ptr;
      size_t length;
    } ptrAndLength;
    const IFormatObject *formatObject;
    char character;
    unsigned int decUI;
    int decI;
    const unsigned long *decUL;
    const long *decL;
    const unsigned long long *decULL;
    const long long *decLL;
    const u64 *uHex;
  };

  /// LHS - The prefix in the concatenation, which may be uninitialized for
  /// Null or Empty kinds.
  Child LHS;

  /// RHS - The suffix in the concatenation, which may be uninitialized for
  /// Null or Empty kinds.
  Child RHS;

  /// LHSKind - The NodeKind of the left hand side, \see getLHSKind().
  NodeKind LHSKind = EmptyKind;

  /// RHSKind - The NodeKind of the right hand side, \see getRHSKind().
  NodeKind RHSKind = EmptyKind;

  /// Construct a nullary twine; the kind must be NullKind or EmptyKind.
  explicit Twine(NodeKind Kind) : LHSKind(Kind) {
    exi_assert(isNullary(), "Invalid kind!");
  }

  /// Construct a binary twine.
  explicit Twine(const Twine &LHS, const Twine &RHS)
      : LHSKind(TwineKind), RHSKind(TwineKind) {
    this->LHS.twine = &LHS;
    this->RHS.twine = &RHS;
    exi_assert(isValid(), "Invalid twine!");
  }

  /// Construct a twine from explicit values.
  explicit Twine(Child LHS, NodeKind LHSKind, Child RHS, NodeKind RHSKind)
      : LHS(LHS), RHS(RHS), LHSKind(LHSKind), RHSKind(RHSKind) {
    exi_assert(isValid(), "Invalid twine!");
  }

  /// Check for the null twine.
  bool isNull() const {
    return getLHSKind() == NullKind;
  }

  /// Check for the empty twine.
  bool isEmpty() const {
    return getLHSKind() == EmptyKind;
  }

  /// Check if this is a nullary twine (null or empty).
  bool isNullary() const {
    return isNull() || isEmpty();
  }

  /// Check if this is a unary twine.
  bool isUnary() const {
    return getRHSKind() == EmptyKind && !isNullary();
  }

  /// Check if this is a binary twine.
  bool isBinary() const {
    return getLHSKind() != NullKind && getRHSKind() != EmptyKind;
  }

  /// Check if this is a valid twine (satisfying the invariants on
  /// order and number of arguments).
  bool isValid() const {
    // Nullary twines always have Empty on the RHS.
    if (isNullary() && getRHSKind() != EmptyKind)
      return false;

    // Null should never appear on the RHS.
    if (getRHSKind() == NullKind)
      return false;

    // The RHS cannot be non-empty if the LHS is empty.
    if (getRHSKind() != EmptyKind && getLHSKind() == EmptyKind)
      return false;

    // A twine child should always be binary.
    if (getLHSKind() == TwineKind &&
        !LHS.twine->isBinary())
      return false;
    if (getRHSKind() == TwineKind &&
        !RHS.twine->isBinary())
      return false;

    return true;
  }

  /// Get the NodeKind of the left-hand side.
  NodeKind getLHSKind() const { return LHSKind; }

  /// Get the NodeKind of the right-hand side.
  NodeKind getRHSKind() const { return RHSKind; }

  /// Print one child from a twine.
  void printOneChild(raw_ostream &OS, Child Ptr, NodeKind Kind) const;

  /// Print the representation of one child from a twine.
  void printOneChildRepr(raw_ostream &OS, Child Ptr,
                         NodeKind Kind) const;

public:
  /// @name Constructors
  /// @{

  /// Construct from an empty string.
  /*implicit*/ Twine() {
    exi_assert(isValid(), "Invalid twine!");
  }

  Twine(const Twine &) = default;

  /// Construct from a C string.
  ///
  /// We take care here to optimize "" into the empty twine -- this will be
  /// optimized out for string constants. This allows Twine arguments have
  /// default "" values, without introducing unnecessary string constants.
  /*implicit*/ Twine(const char *S) {
    if (S[0] != '\0') {
      LHS.cString = S;
      LHSKind = CStringKind;
    } else
      LHSKind = EmptyKind;

    exi_assert(isValid(), "Invalid twine!");
  }
  /// Delete the implicit conversion from nullptr as Twine(const char *)
  /// cannot take nullptr.
  /*implicit*/ Twine(std::nullptr_t) = delete;

  /// Construct from an String.
  /*implicit*/ Twine(const String &S) : LHSKind(StdStringKind) {
    LHS.stdString = &S;
    exi_assert(isValid(), "Invalid twine!");
  }

#if EXI_CUSTOM_STRREF
  /// Construct from an std::string_view by converting it to a pointer and
  /// length.  This handles string_views on a pure API basis, and avoids
  /// storing one (or a pointer to one) inside a Twine, which avoids problems
  /// when mixing code compiled under various C++ standards.
  /*implicit*/ Twine(const std::string_view &S)
      : LHSKind(PtrAndLengthKind) {
    LHS.ptrAndLength.ptr = S.data();
    LHS.ptrAndLength.length = S.length();
    exi_assert(isValid(), "Invalid twine!");
  }
#endif

  /// Construct from a StrRef.
  /*implicit*/ Twine(const StrRef &S) : LHSKind(PtrAndLengthKind) {
    LHS.ptrAndLength.ptr = S.data();
    LHS.ptrAndLength.length = S.size();
    exi_assert(isValid(), "Invalid twine!");
  }

  /// Construct from a StringLiteral.
  /*implicit*/ Twine(const StringLiteral &S)
      : LHSKind(StringLiteralKind) {
    LHS.ptrAndLength.ptr = S.data();
    LHS.ptrAndLength.length = S.size();
    exi_assert(isValid(), "Invalid twine!");
  }

  /// Construct from a SmallStr.
  /*implicit*/ Twine(const SmallVecImpl<char> &S)
      : LHSKind(PtrAndLengthKind) {
    LHS.ptrAndLength.ptr = S.data();
    LHS.ptrAndLength.length = S.size();
    exi_assert(isValid(), "Invalid twine!");
  }

  /// Construct from a formatv_object_base.
  /*implicit*/ Twine(const IFormatObject &Fmt)
      : LHSKind(FormatObjectKind) {
    LHS.formatObject = &Fmt;
    exi_assert(isValid(), "Invalid twine!");
  }

  /// Construct from a char.
  explicit Twine(char Val) : LHSKind(CharKind) {
    LHS.character = Val;
  }

  /// Construct from a signed char.
  explicit Twine(signed char Val) : LHSKind(CharKind) {
    LHS.character = static_cast<char>(Val);
  }

  /// Construct from an unsigned char.
  explicit Twine(unsigned char Val) : LHSKind(CharKind) {
    LHS.character = static_cast<char>(Val);
  }

  /// Construct a twine to print \p Val as an unsigned decimal integer.
  explicit Twine(unsigned Val) : LHSKind(DecUIKind) {
    LHS.decUI = Val;
  }

  /// Construct a twine to print \p Val as a signed decimal integer.
  explicit Twine(int Val) : LHSKind(DecIKind) {
    LHS.decI = Val;
  }

  /// Construct a twine to print \p Val as an unsigned decimal integer.
  explicit Twine(const unsigned long &Val) : LHSKind(DecULKind) {
    LHS.decUL = &Val;
  }

  /// Construct a twine to print \p Val as a signed decimal integer.
  explicit Twine(const long &Val) : LHSKind(DecLKind) {
    LHS.decL = &Val;
  }

  /// Construct a twine to print \p Val as an unsigned decimal integer.
  explicit Twine(const unsigned long long &Val) : LHSKind(DecULLKind) {
    LHS.decULL = &Val;
  }

  /// Construct a twine to print \p Val as a signed decimal integer.
  explicit Twine(const long long &Val) : LHSKind(DecLLKind) {
    LHS.decLL = &Val;
  }

  // FIXME: Unfortunately, to make sure this is as efficient as possible we
  // need extra binary constructors from particular types. We can't rely on
  // the compiler to be smart enough to fold operator+()/concat() down to the
  // right thing. Yet.

  /// Construct as the concatenation of a C string and a StrRef.
  /*implicit*/ Twine(const char *LHS, const StrRef &RHS)
      : LHSKind(CStringKind), RHSKind(PtrAndLengthKind) {
    this->LHS.cString = LHS;
    this->RHS.ptrAndLength.ptr = RHS.data();
    this->RHS.ptrAndLength.length = RHS.size();
    exi_assert(isValid(), "Invalid twine!");
  }

  /// Construct as the concatenation of a StrRef and a C string.
  /*implicit*/ Twine(const StrRef &LHS, const char *RHS)
      : LHSKind(PtrAndLengthKind), RHSKind(CStringKind) {
    this->LHS.ptrAndLength.ptr = LHS.data();
    this->LHS.ptrAndLength.length = LHS.size();
    this->RHS.cString = RHS;
    exi_assert(isValid(), "Invalid twine!");
  }

  /// Since the intended use of twines is as temporary objects, assignments
  /// when concatenating might cause undefined behavior or stack corruptions
  Twine &operator=(const Twine &) = delete;

  /// Create a 'null' string, which is an empty string that always
  /// concatenates to form another empty string.
  static Twine createNull() {
    return Twine(NullKind);
  }

  /// @}
  /// @name Numeric Conversions
  /// @{

  // Construct a twine to print \p Val as an unsigned hexadecimal integer.
  static Twine utohexstr(const u64 &Val) {
    Child LHS, RHS;
    LHS.uHex = &Val;
    RHS.twine = nullptr;
    return Twine(LHS, UHexKind, RHS, EmptyKind);
  }

  /// @}
  /// @name Predicate Operations
  /// @{

  /// Check if this twine is trivially empty; a false return value does not
  /// necessarily mean the twine is empty.
  bool isTriviallyEmpty() const {
    return isNullary();
  }

  /// Check if this twine is guaranteed to refer to single string literal.
  bool isSingleStringLiteral() const {
    return isUnary() && getLHSKind() == StringLiteralKind;
  }

  /// Return true if this twine can be dynamically accessed as a single
  /// StrRef value with getSingleStrRef().
  bool isSingleStrRef() const {
    if (getRHSKind() != EmptyKind)
      return false;
    switch (getLHSKind()) {
    case EmptyKind:
    case CStringKind:
    case StdStringKind:
    case PtrAndLengthKind:
    case StringLiteralKind:
      return true;
    default:
      return false;
    }
  }

  /// @}
  /// @name String Operations
  /// @{

  Twine concat(const Twine &Suffix) const;

  /// @}
  /// @name Output & Conversion.
  /// @{

  /// Return the twine contents as a String.
  String str() const;

  /// Append the concatenated string into the given SmallStr or SmallVec.
  void toVector(SmallVecImpl<char> &Out) const;

  /// This returns the twine as a single StrRef.  This method is only valid
  /// if isSingleStrRef() is true.
  StrRef getSingleStrRef() const {
    exi_assert(isSingleStrRef(), "This cannot be had as a single stringref!");
    switch (getLHSKind()) {
    default: exi_unreachable("Out of sync with isSingleStrRef");
    case EmptyKind:
      return StrRef();
    case CStringKind:
      return StrRef(LHS.cString);
    case StdStringKind:
      return StrRef(*LHS.stdString);
    case PtrAndLengthKind:
    case StringLiteralKind:
      return StrRef(LHS.ptrAndLength.ptr, LHS.ptrAndLength.length);
    }
  }

  /// This returns the twine as a single StrRef if it can be
  /// represented as such. Otherwise the twine is written into the given
  /// SmallVec and a StrRef to the SmallVec's data is returned.
  StrRef toStrRef(SmallVecImpl<char> &Out) const {
    if (isSingleStrRef())
      return getSingleStrRef();
    toVector(Out);
    return StrRef(Out.data(), Out.size());
  }

  /// This returns the twine as a single null terminated StrRef if it
  /// can be represented as such. Otherwise the twine is written into the
  /// given SmallVec and a StrRef to the SmallVec's data is returned.
  ///
  /// The returned StrRef's size does not include the null terminator.
  StrRef toNullTerminatedStrRef(SmallVecImpl<char> &Out) const;

  /// Write the concatenated string represented by this twine to the
  /// stream \p OS.
  void print(raw_ostream &OS) const;

  /// Write the representation of this twine to the stream \p OS.
  void printRepr(raw_ostream &OS) const;

#if !defined(NDEBUG) || defined(EXI_ENABLE_DUMP)
  /// Dump the concatenated string represented by this twine to stderr.
  void dump() const;

  /// Dump the representation of this twine to stderr.
  void dumpRepr() const;
#endif

  /// @}
};

/// @name Twine Inline Implementations
/// @{

inline Twine Twine::concat(const Twine &Suffix) const {
  // Concatenation with null is null.
  if (isNull() || Suffix.isNull())
    return Twine(NullKind);

  // Concatenation with empty yields the other side.
  if (isEmpty())
    return Suffix;
  if (Suffix.isEmpty())
    return *this;

  // Otherwise we need to create a new node, taking care to fold in unary
  // twines.
  Child NewLHS, NewRHS;
  NewLHS.twine = this;
  NewRHS.twine = &Suffix;
  NodeKind NewLHSKind = TwineKind, NewRHSKind = TwineKind;
  if (isUnary()) {
    NewLHS = LHS;
    NewLHSKind = getLHSKind();
  }
  if (Suffix.isUnary()) {
    NewRHS = Suffix.LHS;
    NewRHSKind = Suffix.getLHSKind();
  }

  return Twine(NewLHS, NewLHSKind, NewRHS, NewRHSKind);
}

inline Twine operator+(const Twine &LHS, const Twine &RHS) {
  return LHS.concat(RHS);
}

/// Additional overload to guarantee simplified codegen; this is equivalent to
/// concat().

inline Twine operator+(const char *LHS, const StrRef &RHS) {
  return Twine(LHS, RHS);
}

/// Additional overload to guarantee simplified codegen; this is equivalent to
/// concat().

inline Twine operator+(const StrRef &LHS, const char *RHS) {
  return Twine(LHS, RHS);
}

inline raw_ostream &operator<<(raw_ostream &OS, const Twine &RHS) {
  RHS.print(OS);
  return OS;
}

/// @}

inline namespace ops {

ALWAYS_INLINE Twine operator""_twine(const char* Str, usize Size) {
  return Twine(StrRef(Str, Size));
}

#if defined(__cpp_char8_t)
ALWAYS_INLINE Twine operator""_twine(const char8_t* Str, usize Size) {
  return Twine(StrRef(Str, Size));
}
#endif

} // namespace ops

} // namespace exi
