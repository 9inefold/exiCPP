//===- exi/Decode/BodyDecoder.hpp ------------------------------------===//
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
/// This file implements decoding of the EXI body from a stream.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/ArrayRef.hpp>
#include <core/Common/Option.hpp>
#include <core/Support/Logging.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/ExiHeader.hpp>
#include <exi/Decode/StringTable.hpp>
#include <exi/Decode/HeaderDecoder.hpp>
#include <exi/Decode/Deserializer.hpp>
#include <exi/Decode/UnifyBuffer.hpp>
#include <exi/Grammar/DecoderSchema.hpp>
#include <exi/Stream/OrderedReader.hpp>

#define DEBUG_TYPE "BodyDecoder"
#define DECODER_LOG_POSITIONS 1

// Currently ~2 seconds slower on the large tests. Seems like it messes with the
// OrderedReader's IBP hits? Only the virtual functions are affected, the rest
// actually spend *less* time running. May be viable with Schema integration?
#if 0 && EXI_ENABLE_UNSTABLE_FEATURES && defined(__GNUC__)
# define EXI_DECODER_COMPUTED_GOTO 1
# if !defined(__clang__)
/// Can be used to annotate a label.
#  define LABEL_ANNOTATE(...) __VA_OPT__(__attribute__((__VA_ARGS__));)
# else
/// Label attributes are not available on clang.
#  define LABEL_ANNOTATE(...)
# endif
#endif

#if DECODER_LOG_POSITIONS
// FIXME: Update if needed...
# define LOG_POSITION(...)                                                    \
  LOG_EXTRA("@[{}]:", ((__VA_ARGS__)->Reader->bitPos()))
#else
# define LOG_POSITION(...) ((void)(0))
#endif

namespace exi {

struct DecoderFlags {
  /// If the stream was set externally.
  bool SetReader : 1 = false;
  /// If the header has already been "parsed".
  bool DidHeader : 1 = false;
  /// If init has already been run.
  bool DidInit : 1 = false;
};

/// The EXI decoding processor.
/// FIXME: Split this up into more implementations.
class ExiDecoder {
  friend class decode::Schema::Get;
  /// The provided Header.
  ExiHeader Header = {};
  /// The provided `OrderedReader`.
  OrdReader Reader;
  /// A BumpPtrAllocator for processor internals.
  exi::BumpPtrAllocator BP;
  /// The table holding decoded string values (QNames, LocalNames, etc.)
  decode::StringTable Strings;
  /// The schema for the current document.
  /// TODO: Add SchemaResolver...
  Box<decode::Schema> CurrentSchema;
  // The grammar stack is now stored in the schema.

  /// The stream used for diagnostics.
  Option<raw_ostream&> OS;
  /// State of the decoder in terms of progression.
  DecoderFlags Flags = {};
  /// Preserve options.
  ExiOptions::PreserveOpts Preserve = {};

public:
  ExiDecoder(Option<raw_ostream&> OS = std::nullopt) : OS(OS) {}
  ExiDecoder(MaybeBox<ExiOptions> Opts, Option<raw_ostream&> OS = std::nullopt);
  ~ExiDecoder() { os().flush(); }

  /// Get the state flags.
  DecoderFlags flags() const { return Flags; }
  /// Returns if the header was successfully decoded.
  bool didHeader() const { return Flags.DidHeader; }

  /// Returns the stream used for diagnostics.
  raw_ostream& os() const;
  /// Diagnoses errors in the current context.
  void diagnose(ExiError E, bool Force = false) const;
  /// Diagnoses errors in the current context, then returns.
  ExiError diagnoseme(ExiError E) const {
    this->diagnose(E);
    return E;
  }

  ////////////////////////////////////////////////////////////////////////
  // Initialization

  /// Returns an error if the reader is empty.
  bool isReaderInitialized() const { return !Reader.empty(); }
  /// Returns an error if the reader is empty.
  ExiError assumeReaderIsUninitialized() const;
  /// Sets options out-of-band.
  ExiError setOptions(MaybeBox<ExiOptions> Opts);
  /// Sets reader out-of-band. Options must be provided.
  ExiError setReader(UnifiedBuffer Buffer);

  /// Decodes the header from the provided buffer.
  /// Defined in `HeaderDecoder.cpp`.
  ExiError decodeHeader(UnifiedBuffer Buffer);
  /// Decodes the body from the current stream.
  ExiError decodeBody();
  /// Decodes the body from the current stream with the provided serializer.
  template <std::derived_from<Deserializer> SType>
  ExiError decodeBody(SType* S) {
    if (S == nullptr) {
      // TODO: Allow defaulting in permissive mode?
      LOG_ERROR("Deserializer cannot be null!");
      return ErrorCode::kInvalidEXIInput;
    }

    if (ExiError E = prepareForDecoding())
      return E;

    return this->decodeBodyImpl<SType>(S);
  }

protected:
  /// Initializes StringTable and Schema.
  ExiError init();
  /// Verifies initialization has been completed.
  ExiError prepareForDecoding();

  /// Decodes the body from the current stream with the provided serializer.
  template <typename SType> ExiError decodeBodyImpl(Deserializer* S) {
#if EXI_DECODER_COMPUTED_GOTO
    ExiError E = this->decodeEventLoop<SType>(S);
    if (E == ExiError::DONE)
      return ExiError::OK;
    return E;
#else
    while (Reader->hasData()) {
      ExiError E = this->decodeEvent<SType>(S);
      if EXI_LIKELY(E == ExiError::OK)
        continue;
      else if (E == ExiError::DONE)
        break;
      // Some other error code.
      return E;
    }

    return ExiError::OK;
#endif
  }

#if EXI_DECODER_COMPUTED_GOTO
  /// Decodes events and dispatches in a loop.
  template <typename SType> ExiError decodeEventLoop(Deserializer* S) {
    // Add an extra case with `EventTerm::Void` to get an "error handler" case.
    using DispatchTableType = EnumeratedArray<void*, EventTerm, EventTerm::Void>;
    static_assert(DispatchTableType::size() == 18,
                  "DispatchTable is out of sync! This must be kept up to date "
                  "with EventTerm to function correctly.");
    static constexpr DispatchTableType DispatchTable {
      /* Document */
      //&&caseSD, &&caseED,
      &&caseRare, &&caseRare,
      /* Element */
      &&caseSE, &&caseSE, &&caseSE,
      &&caseEE,
      &&caseAT, &&caseAT, &&caseAT,
      &&caseCH, &&caseCH,
      &&caseNS,
      /* Uncommon */
      &&caseRare, //&&caseCM,
      &&caseRare, //&&casePI,
      &&caseRare, //&&caseDT,
      &&caseRare, //&&caseER,
      &&caseRare, //&&caseSC,
      &&caseHalt
    };

    EventUID Event = CurrentSchema->decode(this);
    ExiError Out = ExiError::OK;

# define HandleEvent(CODE, ARGS...) do {                                      \
  Out = this->handle##CODE<SType>(ARGS);                                      \
  goto caseNext;                                                              \
} while (false)
# define GotoNextEvent() do {                                                 \
  LOG_POSITION(this);                                                         \
  Event = CurrentSchema->decode(this);                                        \
  goto *DispatchTable[Event.getTerm()];                                       \
} while (false)
# define DispatchMarkCase(ATTR, CODE, ...)                                    \
case##CODE:                                                                   \
  LABEL_ANNOTATE(ATTR)                                                        \
  HandleEvent(CODE __VA_OPT__(,) __VA_ARGS__);                                \
  GotoNextEvent();
# define DispatchCase(CODE, ...)                                              \
  DispatchMarkCase(, CODE __VA_OPT__(,) __VA_ARGS__)

    GotoNextEvent(); {
      caseNext:
        LABEL_ANNOTATE(hot) {
        if EXI_NEVER(Out != ExiError::OK)
          return Out;
        GotoNextEvent();
      }
      /* ELEMENTS */
      // Start Element (*)
      // Start Element (uri:*)
      // Start Element (qname)
      DispatchMarkCase(hot, SE, S, Event)
      // End Element
      DispatchMarkCase(hot, EE, S, Event)
      // Attribute (*, value)
      // Attribute (uri:*, value)
      // Attribute (qname, value)
      DispatchCase(AT, S, Event)
      // Characters (value)
      // Characters (external-value)
      DispatchCase(CH, S, Event)
      // Namespace Declaration (uri, prefix, local-element-ns)
      DispatchCase(NS, S, Event)
      /* UNCOMMON */
      caseRare:
        LABEL_ANNOTATE(cold)
        Out = this->dispatchUncommonEvent(S, Event);
        goto caseNext;
      caseHalt:
        LABEL_ANNOTATE(cold)
        return Out;
    }

# undef HandleEvent
# undef GotoNextEvent
# undef DispatchMarkCase
# undef DispatchCase
    exi_unreachable("Fell through DispatchTable?");
  }
#else
  /// Decodes events and then dispatches.
  template <typename SType> EXI_HOT ExiError decodeEvent(Deserializer* S) {
    LOG_POSITION(this);
    const EventUID Event = CurrentSchema->decode(this);

    switch (Event.getTerm()) {
    case EventTerm::SE:       // Start Element (*)
    case EventTerm::SEUri:    // Start Element (uri:*)
    case EventTerm::SEQName:  // Start Element (qname)
      return this->handleSE<SType>(S, Event);
    case EventTerm::EE:       // End Element
      return this->handleEE<SType>(S, Event);
    case EventTerm::AT:       // Attribute (*, value)
    case EventTerm::ATUri:    // Attribute (uri:*, value)
    case EventTerm::ATQName:  // Attribute (qname, value)
      return this->handleAT<SType>(S, Event);
    case EventTerm::NS:       // Namespace Declaration (uri, prefix, local-element-ns)
      return this->handleNS<SType>(S, Event);
    case EventTerm::CH:       // Characters (value)
    case EventTerm::CHExtern: // Characters (external-value)
      return this->handleCH<SType>(S, Event);
    default:
      return this->dispatchUncommonEvent(S, Event);
    }
  }
#endif

  /// Dispatches less common events.
  EXI_COLD ExiError dispatchUncommonEvent(Deserializer* S, EventUID Event);

  ////////////////////////////////////////////////////////////////////////
  // Terms

  [[maybe_unused]] EXI_COLD ExiError handleSD(Deserializer* S);
  [[maybe_unused]] EXI_COLD ExiError handleED(Deserializer* S);

  // Start Element (*)
  // Start Element (uri:*)
  // Start Element (qname)
  template <typename SType> ExiError handleSE(Deserializer* S, EventUID Event) {
    const QName Name = this->getQName(Event);
    LOG_EXTRA("Decoded SE");
    return static_cast<SType*>(S)->SE(Name);
  }

  template <typename SType> ExiError handleEE(Deserializer* S, EventUID Event) {
    if (!Event.hasQName()) {
#if EXI_LOGGING
      LOG_EXTRA("Decoded EE");
      if (hasDbgLogLevel(INFO))
        dbgs() << '\n';
#endif
      return ExiError::OK;
    }

    const QName Name = this->getQName(Event);
    LOG_INFO(">> EE[{}:{}]\n", Name.pfx(), Name.name());
    return static_cast<SType*>(S)->EE(Name);
  }

  // Attribute (*, value)
  // Attribute (uri:*, value)
  // Attribute (qname, value)
  template <typename SType> ExiError handleAT(Deserializer* S, EventUID Event) {
    exi_invariant(Event.hasQName());
    Result R = decodeValue(Event.Name);
    if EXI_UNLIKELY(R.is_err())
      return R.error();
    const auto ValueID = *std::move(R);

    const QName Name = this->getQName(Event);
    StrRef Value = Strings.getValue(ValueID);

    LOG_EXTRA("Decoded AT");
    return static_cast<SType*>(S)->AT(Name, Value);
  }

  // Namespace Declaration (uri, prefix, local-element-ns)
  template <typename SType> ExiError handleNS(Deserializer* S, EventUID) {
    Result R = decodeNS();
    if EXI_UNLIKELY(R.is_err())
      return R.error();
    const auto Event = *std::move(R);
    const auto Name = Event.Name;

    StrRef URI = Strings.getURI(Name.URI);
    StrRef Pfx = Strings.getPrefix(Name.URI, Event.Prefix);

    LOG_EXTRA("Decoded NS");
    if EXI_LIKELY(!Event.isLocal())
      return static_cast<SType*>(S)->NS(URI, Pfx);
    else
      return static_cast<SType*>(S)->NS_Local(URI, Pfx, Name.URI);
  }

  // Characters (value)
  template <typename SType> ExiError handleCH(Deserializer* S, EventUID Event) {
    StrRef Value = Strings.getValue(Event);
    LOG_EXTRA("Decoded CH");
    return static_cast<SType*>(S)->CH(Value);
  }

  ExiError handleCM(Deserializer* S);
  EXI_COLD ExiError handlePI(Deserializer* S);
  EXI_COLD ExiError handleDT(Deserializer* S);
  EXI_COLD ExiError handleER(Deserializer* S);

  QName getQName(EventUID Event);
  // TODO: Add optional `UserPrefixLookup*` type.
  StrRef getPfxOrURI(EventUID Event);
  Option<StrRef> tryGetPfx(CompactID URI, CompactID PfxID);

  ////////////////////////////////////////////////////////////////////////
  // Values

  /// Interns a single string with the given allocator.
  // TODO: Make this global? Or maybe integrate into `BumpPtrAllocator`...
  EXI_NO_INLINE static void InternString(BumpPtrAllocator& BP, StrRef& Str) {
    if (Str.empty()) {
      Str = ""_str;
      return;
    }

    const usize Size = Str.size();
    char* Raw = BP.Allocate<char>(Size + 1);
    std::memcpy(Raw, Str.data(), Size);
    Raw[Size] = 0;
    Str = {Raw, Size};
  }

  /// Interns a collection of strings with `BP`.
  EXI_INLINE void internStrings(auto&...Strs) {
    (InternString(this->BP, Strs), ...);
  }

  /// Decodes a QName.
  ExiResult<EventUID> decodeQName();

  /// Decodes a Namespace.
  ExiResult<EventUID> decodeNS();

  /// Decodes a QName URI.
  ExiResult<CompactID> decodeURI();

  /// Decodes a QName LocalName.
  /// @param URI The bucket to search in.
  ExiResult<CompactID> decodeName(CompactID URI);

  /// Same as `decodeName`, decodes a QName LocalName.
  ALWAYS_INLINE auto decodeLocalName(CompactID URI) {
    return this->decodeName(URI);
  }

  /// Decodes a QName Prefix, if `Preserve.Prefixes` is enabled.
  /// @param URI The bucket to search in.
  ExiResult<Option<CompactID>> decodePfxQ(CompactID URI);

  /// Decodes a NS Prefix, `Preserve.Prefixes` must be enabled.
  /// @param URI The bucket to search in.
  ExiResult<CompactID> decodePfx(CompactID URI);

  /// Decodes a Value.
  ExiResult<EventUID> decodeValue(CompactID URI, CompactID Name) {
    return this->decodeValue(SmallQName::NewQName(URI, Name));
  }
  /// Decodes a Value.
  ExiResult<EventUID> decodeValue(SmallQName Name);

  /// @brief Decodes an encoded string with the default character set.
  /// @return An owning `String`, or an error.
  /// @overload
  ExiResult<String> decodeString();

  /// @brief Decodes an encoded string with the default character set.
  /// @param Storage Where the string will be stored.
  /// @return An non-owning `StrRef`, or an error.
  ExiResult<StrRef> decodeString(SmallVecImpl<char>& Storage) {
    return Reader->decodeString(Storage);
  }

  /// @brief Decodes a string with with the size already decoded.
  /// @param Size The length of the string.
  /// @param Storage Where the string will be stored.
  /// @return An non-owning `StrRef`, or an error.
  ExiResult<StrRef> readString(u64 Size, SmallVecImpl<char>& Storage) {
    // FIXME: LOG_POSITION(this);
    return Reader->readString(Size, Storage);
  }
};

} // namespace exi

#undef DEBUG_TYPE
#undef LABEL_ANNOTATE
