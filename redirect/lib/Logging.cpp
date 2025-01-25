//===- Logging.cpp --------------------------------------------------===//
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
/// This file defines functions for logging activity.
///
//===----------------------------------------------------------------===//

#include <Logging.hpp>
#include <ArrayRef.hpp>
#include <ArgList.hpp>
#include <Globals.hpp>
#include <Strings.hpp>
#include <utility>

#undef SETUP_PRINTF

using namespace re;

static constexpr usize kBufSize = 4096 * 4;
static char LogBuffer[kBufSize + 2];
static usize LogSize = 0;

static inline bool IsLogFull() noexcept {
  re_assert(LogSize < sizeof(LogBuffer));
  return UNLIKELY(LogSize == kBufSize);
}

static void WritePadding(const char C, i64 Len) {
  if (IsLogFull())
    return;
  if (C == '\0' || Len <= 0)
    return;
  
  const usize FinalSize = re::min(LogSize + Len, kBufSize);
  const usize WriteSize = FinalSize - LogSize;

  (void) Memset(LogBuffer + LogSize, static_cast<u8>(C), WriteSize);
  ::LogSize = FinalSize;
  ::LogBuffer[LogSize] = '\0';
}

static void WriteToLog(const char* Str, i64 Len) {
  if (IsLogFull())
    return;
  if (!Str || Len <= 0)
    return;
  
  const usize FinalSize = re::min(LogSize + Len, kBufSize);
  const usize WriteSize = FinalSize - LogSize;

  (void) Memcpy(LogBuffer + LogSize, Str, WriteSize);
  ::LogSize = FinalSize;
  ::LogBuffer[LogSize] = '\0';
}

static void WriteToLog(const char* Str) {
  if (!Str || IsLogFull())
    return;
  const usize Cap = (kBufSize - LogSize);
  return WriteToLog(Str, Strnlen(Str, Cap));
}

static void WriteToLog(char C) {
  return WriteToLog(&C, 1);
}

//////////////////////////////////////////////////////////////////////////
// Parsing

namespace {

struct ParseFlags {
  bool LeftJustified : 1; // `-`
  bool ShowSigned : 1;    // `+`
  bool PrependSpace : 1;  // ` `
  bool AltForm : 1;       // `#`
  bool PadZero : 1;       // `0`
};

enum class LengthKind { none, hh, h, l, ll, j, z, t, L };

struct ParseSection {
  ArrayRef<char> Data;
  bool IsSpec = false;

  ParseFlags Flags = {};
  int MinWidth     = 0;
  int Precision    = -1;

  LengthKind LenK  = LengthKind::none;
  char Conv        = '\0';
};

class Parser {
  const char* Fmt;
  usize Pos = 0;
  H::ArgList Args;
public:
  inline Parser(const char* Fmt, H::ArgList& Args) : 
   Fmt(Fmt), Args(Args) {
  }

  inline ParseSection getNextSection() {
    ParseSection PS;
    const usize StartPos = Pos;

    if (Fmt[StartPos] != '%') {
      // Not a spec.
      PS.IsSpec = false;
      while (Fmt[Pos] != '%' && Fmt[Pos] != '\0')
        ++Pos;
      PS.Data = {Fmt + StartPos, Pos - StartPos};
      return PS;
    }

    // Is a spec (begins with %).
    PS.IsSpec = true;
    ++Pos;

    // Handle flags.
    PS.Flags = parseFlags(&Pos);

    // Handle width.
    if (Fmt[Pos] == '*') {
      PS.MinWidth = next<int>();
      ++Pos;
    } else if (H::is_digit(Fmt[Pos])) {
      auto [Val, Len] = parseInt<>(Pos);
      PS.MinWidth = Val;
      Pos += Len;
    }
    if (PS.MinWidth < 0) {
      PS.MinWidth
        = -re::max(PS.MinWidth, INT_MIN + 1);
      PS.Flags.LeftJustified = true;
    }

    // Handle precision.
    if (Fmt[Pos] == '.') {
      ++Pos;
      PS.Precision = 0;

      if (Fmt[Pos] == '*') {
        PS.Precision = next<int>();
        ++Pos;
      } else if (H::is_digit(Fmt[Pos])) {
        auto [Val, Len] = parseInt<>(Pos);
        PS.Precision = Val;
        Pos += Len;
      }
      if (PS.Precision < 0)
        // Always set back to -1... for sanity.
        PS.Precision = -1;
    }

    PS.LenK = parseLength(&Pos);
    PS.Conv = Fmt[Pos++];
    PS.Data = {Fmt + StartPos, Pos - StartPos};

    return PS;
  }

  template <typename T>
  inline T next() {
    return Args.template next<T>();
  }

private:
  /// Radix assumed to be 10.
  /// @return `[Value, ParseSize]`.
  template <bool HandleEdgeCases = false>
  std::pair<int, usize> parseInt(const usize Off) const {
    usize Curr = Off;
    if constexpr (HandleEdgeCases) {
      while (H::is_space(Fmt[Curr]))
        ++Curr;
    }
    
    bool IsPositive = true;
    if constexpr (HandleEdgeCases) {
      if (Fmt[Curr] == '-') {
        IsPositive = false;
        ++Curr;
      }
    }
    
    int Out = 0;
    bool IsNumber = false;
    while (H::is_digit(Fmt[Curr])) {
      int Digit = (Fmt[Curr] - '0');

      IsNumber = true;
      ++Curr;

      Out *= 10;
      Out += Digit;
    }

    usize OutLen = IsNumber ? (Curr - Off) : 0;
    return {(IsPositive ? Out : -Out), OutLen};
  }

  inline ParseFlags parseFlags(usize* CurrPos) {
    re_assert(CurrPos != nullptr);
    bool HasFlag = true;
    ParseFlags Flags {};
    while (HasFlag) {
      switch (Fmt[*CurrPos]) {
      case '-':
        Flags.LeftJustified = true;
        break;
      case '+':
        Flags.ShowSigned = true;
        break;
      case ' ':
        Flags.PrependSpace = true;
        break;
      case '#':
        Flags.AltForm = true;
        break;
      case '0':
        Flags.PadZero = true;
        break;
      default:
        HasFlag = false;
      }
      // Advance if we still have flags.
      if (HasFlag)
        ++*CurrPos;
    }
    return Flags;
  }

  inline LengthKind parseLength(usize* CurrPos) {
    re_assert(CurrPos != nullptr);
    switch (Fmt[*CurrPos]) {
    case 'h':
      if (Fmt[*CurrPos + 1] == 'h') {
        *CurrPos += 2;
        return LengthKind::hh;
      } else {
        ++*CurrPos;
        return LengthKind::h;
      }
    case 'l':
      if (Fmt[*CurrPos + 1] == 'l') {
        *CurrPos += 2;
        return LengthKind::ll;
      } else {
        ++*CurrPos;
        return LengthKind::l;
      }
    case 'j':
      ++*CurrPos;
      return LengthKind::j;
    case 'z':
      ++*CurrPos;
      return LengthKind::z;
    case 't':
      ++*CurrPos;
      return LengthKind::t;
    case 'L':
      ++*CurrPos;
      return LengthKind::L;
    default:
      return LengthKind::none;
    }
  }
};

} // namespace `anonymous`

//////////////////////////////////////////////////////////////////////////
// Integer Stuff

template <typename BaseInt, bool IsSigned>
using select_int_impl_t = std::conditional_t<
  IsSigned,
  std::make_signed_t<BaseInt>,
  std::make_unsigned_t<BaseInt>
>;

template <typename BaseInt, bool IsSigned>
using select_int_t = select_int_impl_t<std::conditional_t<
  (sizeof(BaseInt) >= sizeof(int)),
  BaseInt, int
>, IsSigned>;

static inline constexpr usize ComputeMaxBufferSize() {
  constexpr auto kMaxDigits = []() -> usize {
    constexpr auto kFloorLog2 = [](usize num) -> usize {
      usize i = 0;
      for (; num > 1; num /= 2)
        ++i;
      return i;
    };
    constexpr usize kBitsPerDigit = kFloorLog2(8);
    return ((sizeof(uintmax_t) * 8 + (kBitsPerDigit - 1)) / kBitsPerDigit);
  };
  constexpr usize kDigitSize = re::max(kMaxDigits(), usize(1));
  constexpr usize kSignSize = 1;
  constexpr usize kPrefixSize = 2;
  return kDigitSize + kSignSize + kPrefixSize;
}

static inline auto GetRadixAndSignedness(
 const ParseSection& PS) -> std::pair<int, bool> {
  switch (PS.Conv) {
  case 'd':
  case 'i':
    return {10, true};
  case 'u':
    return {10, false};
  case 'o':
    return {8, false};
  case 'x':
  case 'X':
  case 'p':
    return {16, false};
  default:
    return {0, true};
  }
}

template <bool Signed>
static auto ExtractIntImpl(const ParseSection& PS, Parser& Args)
 -> std::conditional_t<Signed, intmax_t, uintmax_t> {
  using enum LengthKind;
  if (PS.Conv == 'p') {
    re_assert(!Signed, "Must be unsigned!");
    auto Ptr = Args.next<void*>();
    return reinterpret_cast<uintptr_t>(Ptr);
  }

  switch (PS.LenK) {
  case hh: return Args.next<select_int_t<char,       Signed>>();
  case h:  return Args.next<select_int_t<short,      Signed>>();
  default: return Args.next<select_int_t<int,        Signed>>();
  case l:  return Args.next<select_int_t<long,       Signed>>();
  case ll: return Args.next<select_int_t<long long,  Signed>>();
  case j:  return Args.next<select_int_t<intmax_t,   Signed>>();
  case z:  return Args.next<select_int_t<size_t,     Signed>>();
  case t:  return Args.next<select_int_t<ptrdiff_t,  Signed>>();
  }
}

static uintmax_t ExtractInt(
 const ParseSection& PS, ParseFlags& Flags,
 Parser& Args, bool& IsNegative) {
  if (PS.Conv == 'i' || PS.Conv == 'd') {
    intmax_t Out = ExtractIntImpl<true>(PS, Args);
    if (Out < 0) {
      IsNegative = true;
      Out = -Out;
    }
    return static_cast<uintmax_t>(Out);
  }

  // Return unsigned directly.
  Flags.ShowSigned = false;
  Flags.PrependSpace = false;
  IsNegative = false;
  return ExtractIntImpl<false>(PS, Args);
}

//////////////////////////////////////////////////////////////////////////
// Writer

namespace {

class PadHelper {
  const ParseSection& PS;
  usize Padding = 0;
  char ToPad;
public:
  PadHelper(const ParseSection& PS, usize Padding, char Ch = ' ') :
   PS(PS), Padding(Padding), ToPad(Ch) {
    // If the padding is on the left side, write the spaces first.
    if (Padding > 0 && not PS.Flags.LeftJustified) {
      WritePadding(ToPad, Padding);
    }
  }

  ~PadHelper() {
    // If the padding is on the right side, write the spaces last.
    if (Padding > 0 && PS.Flags.LeftJustified) {
      WritePadding(ToPad, Padding);
    }
  }
};

} // namespace `anonymous`

static void Write(ArrayRef<char> Buf) {
  WriteToLog(Buf.data(), Buf.size());
}

static void Write(char C) {
  WriteToLog(&C, 1);
}

static void WriteChar(const ParseSection& PS, Parser& Args) {
  char C = Args.next<unsigned int>();
  constexpr usize kDefaultLen = 1;

  usize Padding
    = PS.MinWidth > kDefaultLen
    ? PS.MinWidth - kDefaultLen : 0;
  // Handles padding
  PadHelper pad(PS, Padding);
  Write(C);
}

static void WriteString(const ParseSection& PS, Parser& Args) {
  usize Len = 0;
  auto* Str = Args.next<const char*>();

  if (Str == nullptr)
    Str = "(null)";
  
  {
    usize Max = kBufSize;
    if (PS.Precision > 0)
      Max = usize(PS.Precision);
    Len = Strnlen(Str, Max);
  }

  usize Padding
    = PS.MinWidth > Len
    ? PS.MinWidth - Len : 0;
  // Handles padding
  PadHelper pad(PS, Padding);
  Write({Str, Len});
}

static ArrayRef<char> WriteInt(
 MutArrayRef<char> Buf, uintmax_t Val,
 char Conv, unsigned Radix) {
  const char* const Chars
    = H::is_upper(Conv)
    ? "0123456789ABCDEF"
    : "0123456789abcdef";
  
  char* Ptr = Buf.end();
  const char* End = &Buf.front();
  do {
    --Ptr;
    unsigned Off = Val % Radix;
    Val /= Radix;
    *Ptr = Chars[Off];
    if (Ptr == End) {
      re_assert(false, "What??");
      break;
    }
  } while (Val > 0);

  return {Ptr, Buf.end()};
}

static void WriteInt(const ParseSection& PS, Parser& Args) {
  auto [Radix, IsSigned] = GetRadixAndSignedness(PS);
  bool IsNegative = false;
  const char a = H::is_lower(PS.Conv) ? 'a' : 'A';

  ParseFlags Flags = PS.Flags;
  auto Val = ExtractInt(PS, Flags, Args, IsNegative);

  constexpr usize kMaxInt = ComputeMaxBufferSize();
  char IntBuf[kMaxInt];
  auto Str = WriteInt({IntBuf, kMaxInt}, Val, PS.Conv, Radix);

  usize DigitsWritten = Str.size();

  char SignChar = 0;
  if (IsNegative)
    SignChar = '-';
  else if (Flags.ShowSigned)
    SignChar = '+'; // .ShowSigned has precedence over .PrependSpace
  else if (Flags.PrependSpace)
    SignChar = ' ';
  
  char Prefix[2] {};
  usize PrefixLen = 0;
  if (Radix == 16 && Flags.AltForm && Val != 0) {
    PrefixLen = 2;
    Prefix[0] = '0';
    Prefix[1] = a + ('x' - 'a');
  } else {
    PrefixLen = !!SignChar;
    Prefix[0] = SignChar;
  }

  int Zeroes = 0;
  int Padding = 0;

  if (PS.Precision < 0) {
    int Pad = static_cast<int>(
      PS.MinWidth - DigitsWritten - PrefixLen);
    if (Flags.PadZero && !Flags.LeftJustified) {
      Zeroes = Pad;
      Padding = 0;
    } else {
      Zeroes = 0;
      Padding = Pad;
    }
  } else {
    if (Val == 0 && PS.Precision == 0)
      DigitsWritten = 0;
    Zeroes = static_cast<int>(PS.Precision - DigitsWritten);
    if (Zeroes < 0)
      Zeroes = 0;
    Padding = static_cast<int>(PS.MinWidth - Zeroes - DigitsWritten - PrefixLen);
  }

  const bool LeadingZero =
    (Zeroes > 0) || (Val == 0 && DigitsWritten != 0);
  if ((PS.Conv == 'o') && Flags.PadZero && !LeadingZero) {
    Zeroes = 1;
    --Padding;
  }

  // Handles padding
  PadHelper pad(PS, Padding);
  if (PrefixLen > 0)
    WriteToLog(Prefix, PrefixLen);
  if (Zeroes > 0)
    WritePadding('0', Zeroes);
  if (DigitsWritten > 0)
    Write(Str);
}

static void HandleSpec(const ParseSection& PS, Parser& Args) {
  if UNLIKELY(!PS.IsSpec) {
    Write(PS.Data);
    return;
  }

  // Don't check for LengthKind::L, no float support.

  switch (PS.Conv) {
  case '%':
    return WriteToLog('%');
  case 'c':
    return WriteChar(PS, Args);
  case 's':
    return WriteString(PS, Args);
  case 'd':
  case 'i':
  case 'u':
  case 'o':
  case 'x':
  case 'X':
  case 'p':
    return WriteInt(PS, Args);
  // case 'p':
  //   return WritePointer(PS, Args);
  default:
    return Write(PS.Data);
  }
}

static void LogMain(const char* Fmt, H::ArgList& Args) {
  Parser parser(Fmt, Args);
  int X = 0;
  for (
   ParseSection PS = parser.getNextSection();
   !PS.Data.empty();
   PS = parser.getNextSection()) {
    if (PS.IsSpec)
      HandleSpec(PS, parser);
    else
      Write(PS.Data);
    if (IsLogFull())
      break;
  }
}

//////////////////////////////////////////////////////////////////////////
// Exposed

static void LogCommon(const char* Pre, const char* Fmt, H::ArgList& Args) {
  if (IsLogFull())
    return;
  if (Pre != nullptr)
    WriteToLog(Pre);
  // Do printf-style formatting.
  LogMain(Fmt, Args);
  // Terminate.
  if (not IsLogFull()) {
    WriteToLog('\n');
  } else {
    ::LogBuffer[kBufSize - 1] = '.';
    ::LogBuffer[kBufSize + 0] = '\n';
    ::LogBuffer[kBufSize + 1] = '\0';
  }
}

#define SETUP_PRINTF(OutList, Format) \
  va_list VList;                      \
  va_start(VList, Format);            \
  H::ArgList OutList(VList);          \
  va_end(VList);                      \

PRINTF_ATTR void re::MiTrace(const char* Fmt, ...) {
  if (!::MIMALLOC_VERBOSE)
    return;
  SETUP_PRINTF(Args, Fmt)
  LogCommon("mimalloc-redirect: trace: ", Fmt, Args);
}

PRINTF_ATTR void re::MiWarn(const char* Fmt, ...) {
  SETUP_PRINTF(Args, Fmt)
  LogCommon("mimalloc-redirect: warning: ", Fmt, Args);
}

PRINTF_ATTR void re::MiError(const char* Fmt, ...) {
  SETUP_PRINTF(Args, Fmt)
  LogCommon("mimalloc-redirect: error: ", Fmt, Args);
}

extern "C" DLL_EXPORT bool mi_allocator_init(const char** Str) {
  if (Str)
    *Str = LogBuffer;
  return NO_PATCH_ERRORS;
}
