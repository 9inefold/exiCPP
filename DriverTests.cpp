//===- DriverTests.cpp ----------------------------------------------===//
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

#include "Driver.hpp"
#include <Common/AlignedInt.hpp>
#include <Common/APSInt.hpp>
#include <Common/EnumArray.hpp>
#include <Common/Box.hpp>
#include <Common/MaybeBox.hpp>
#include <Common/Map.hpp>
#include <Common/PointerUnion.hpp>
#include <Common/Poly.hpp>
#include <Common/Result.hpp>
#include <Common/String.hpp>
#include <Common/SmallStr.hpp>
#include <Common/SmallVec.hpp>
#include <Common/StringSwitch.hpp>
#include <Support/Alignment.hpp>
#include <Support/Allocator.hpp>
#include <Support/Chrono.hpp>
#include <Support/Casting.hpp>
#include <Support/Debug.hpp>
#include <Support/Error.hpp>
#include <Support/Filesystem.hpp>
#include <Support/FmtBuffer.hpp>
#include <Support/Format.hpp>
#include <Support/MemoryBufferRef.hpp>
#include <Support/Process.hpp>
#include <Support/raw_ostream.hpp>

#include <exi/Basic/Bounded.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/NBitInt.hpp>
#include <exi/Basic/ProcTypes.hpp>
#include <exi/Basic/Runes.hpp>
// #include <exi/Stream/BitStreamReader.hpp>
// #include <exi/Stream/BitStreamWriter.hpp>

#include <tuple>
#include <malloc.h>
#include <windows.h>

#include <fmt/format.h>
#include <fmt/chrono.h>
#include <fmt/ranges.h>

// TODO: Tests!!

using namespace exi;
using namespace exi::sys;

#undef  SKIP
#define SKIP if (0)

enum class E1 { A, B, C, D, Last = D };
enum class E2 { A, B, C, D };
enum class E3 { A = 1, B, C, D, Last = D };
enum class E4 { A = 1, B, C, D };

EXI_MARK_ENUM_LAST(E2, D)
EXI_MARK_ENUM_FIRST(E3, A)
EXI_MARK_ENUM_BOUNDS(E4, A, D)

static_assert(EnumArray<int, E1>::size() == 4);
static_assert(EnumArray<int, E2>::size() == 4);
static_assert(EnumArray<int, E3>::size() == 4);
static_assert(EnumArray<int, E4>::size() == 4);

//===----------------------------------------------------------------===//
// Misc
//===----------------------------------------------------------------===//

#if EXI_USE_MIMALLOC
static bool ITestMimallocRedirect(usize Mul) {
  bool Result = true;
  if (!mi_is_redirected())
    return true;
  {
    void* Alloc = malloc(16 * Mul);
    if (void* New = _expand(Alloc, 32 * Mul))
      free(New);
    else
      free(Alloc);
  } {
    void* Alloc = malloc(16 * Mul);
    void* New = realloc(Alloc, 512 * Mul);
    free(New);
  } {
    // void* Alloc = malloc(16 * Mul);
    // void* New = _recalloc(Alloc, 64, 4 * Mul);
    // free(New);
  } {
    constexpr Align A = 32_align;
    void* Alloc = _aligned_malloc(16 * Mul, A.value());
    if (!isAddrAligned(A, Alloc))
      Result = false;
    void* New = _aligned_realloc(Alloc, 64 * Mul, A.value());
    if (!isAddrAligned(A, Alloc))
      Result = false;
    _aligned_free(New);
  }
  return Result;
}

static bool testMimallocRedirect() {
  constexpr usize kMaxMul = 20'000'000;
  usize Mul = 1;

  bool Result = true;
  outs() << "Running tests...\n";
  while (Mul < kMaxMul) {
    if (ITestMimallocRedirect(Mul)) {
      outs() << "Test " << Mul << " passed.\n"; 
    } else {
      outs() << "Test " << Mul << " failed.\n"; 
      Result = false;
    }
    Mul *= 2;
  }

  if (Result)
    outs() << "All tests passed!\n"; 
  return Result;
}

static void printIfInHeap(const void* Ptr) {
  if (mi_is_in_heap_region(Ptr))
    fmt::println("\"{}\" in heap!", Ptr);
  else
    fmt::println("\"{}\" not in heap!", Ptr);
}
#else
# define printIfInHeap(...) (void(0))
#endif // EXI_USE_MIMALLOC

static void miscTests(int Argc, char* Argv[]) {
#if EXI_USE_MIMALLOC
  if (mi_option_is_enabled(mi_option_verbose)) {
    mi_option_disable(mi_option_verbose);
    outs() << '\n';
  }
  if (!mi_is_redirected()) {
    outs() << "Redirection failed.\n";
  } else {
    outs() << "Is redirected!\n";
    testMimallocRedirect();
  }
  outs() << '\n';
#endif // EXI_USE_MIMALLOC

  SmallStr<256> Str;
  fs::current_path(Str);
  outs() << Argv[0] << '\n';
  fmt::println("{}", Str);

  TimePoint<> TP = sys::now();
  fmt::println("TimePoint<>: {}", TP);
  TimePoint<> TP2 = sys::now();
  fmt::println("Duration: {}", (TP2 - TP));
  outs() << TP << ", " << TP2 << '\n';

  {
    auto* P = (char*)exi::allocate_buffer(4096, 16);
    printIfInHeap(P);
    exi::deallocate_buffer(P, 4096, 16);
  }

  std::string SStr;
  wrap_stream(SStr) << "Hello world!\nIt's me!";
  printIfInHeap(SStr.data());

  errs() << "\n\n";
  errs() << "mimalloc: " << Process::GetMallocUsage() << '\n';
  errs() << "malloc:   " << Process::GetStdMallocUsage() << '\n';
}

//===----------------------------------------------------------------===//
// NBitInt
//===----------------------------------------------------------------===//

namespace {
template <bool Sign> struct BitData {
  using IntT = H::NBitIntValueType<Sign>;

  const IntT Converted = 0;
  const u64 AllData = 0;
  const unsigned Bits = 0;

public:
  template <unsigned InBits>
  BitData(NBitIntCommon<Sign, InBits> Val) :
   Converted(Val.data()), AllData(exi::bit_cast<u64>(Val)), Bits(InBits) {
  }
};

template <bool Sign, unsigned Bits>
BitData(NBitIntCommon<Sign, Bits>) -> BitData<Sign>;
} // namespace `anonymous`

template <bool DoByteswap = true>
static void printBitIntData(u64 AllData, const char* Pre = nullptr) {
  using Arr = std::array<u8, sizeof(u64)>;
  if (DoByteswap)
    AllData = exi::byteswap(AllData);
  auto A = std::bit_cast<Arr>(AllData);
  fmt::print("  ");
  if (Pre && *Pre)
    fmt::print("{}: ", Pre);
  for (u8 Byte : A) {
    u8 Hi = (Byte >> 4);
    u8 Lo = (Byte & 0xF);
    fmt::print("{:04b}'{:04b} ", Hi, Lo);
  }
  fmt::println("");
}

template <bool DoByteswap = true, bool Sign>
static void printAllData(BitData<Sign> Data, const char* Pre = nullptr) {
  printBitIntData<DoByteswap>(Data.AllData, Pre);
}

static bool commonChecks(
 BitData<1> I,
 BitData<0> U,
 BitData<1> IZero,
 BitData<0> UZero,
 u64 kBits
) {
  fmt::print("#{:02} | ", I.Bits);
  if (I.AllData != U.AllData) {
    fmt::println("Error: IAllData != UAllData.");
    printAllData(I, "I");
    printAllData(U, "U");
    return false;
  }

  if (I.AllData != kBits) {
    fmt::println("Error: AllData != kBits.");
    printBitIntData(kBits, "kBits");
    printAllData(I, "+Data");
    printAllData(U, "~Data");
    printAllData(IZero, "+Zero");
    printAllData(UZero, "~Zero");
    if (I.Converted != -1)
      fmt::println("  Real value: {}", I.Converted);
    return false;
  }

  fmt::println("Success!");
  // printAllData(I, "Data");
  return true;
}

template <unsigned Bits>
static bool testBits() noexcept {
  using IntT = H::NBitIntValueType<false>;
  constexpr usize MAX_BITS = NBitIntBase::kMaxBits;
  constexpr IntT kBits = (~IntT(0) >> (MAX_BITS - Bits));

  using SInt = ibit<Bits>;
  using UInt = ubit<Bits>;

  SInt I = SInt::FromBits(kBits);
  UInt U = UInt::FromBits(kBits);

  // __builtin_clear_padding(&I);
  // __builtin_clear_padding(&U);

  return commonChecks(
    I, U, SInt(0), UInt(0), kBits);
}

static void runAllTests() {
  constexpr usize MAX_BITS = NBitIntBase::kMaxBits;
  bool Result = [] <usize...II>
   (std::index_sequence<II...>) -> bool {
    bool Out = true;
    ((Out &= testBits<II + 1>()), ...);
    return Out;
  } (std::make_index_sequence<MAX_BITS>{});

  fmt::println("");
  if (Result == true) {
    fmt::println("All tests passed!");
  } else {
    fmt::println("Some tests failed.");
    std::exit(1);
  }
}

//===----------------------------------------------------------------===//
// BitStream
//===----------------------------------------------------------------===//

#if 0
// FIXME: Update tests?

static bool gPrintAPIntTail = true;
static bool gPrintAPWordBounds = false;

static void BitIntTests() {
  {
    using SInt = ibit<4>;
    using UInt = ubit<4>;

    SInt I = 0;
    exi_assert(CheckIntCast<u8>(I));
    exi_assert(CheckIntCast<i8>(I));

    exi_assert(!CheckIntCast<SInt>(0b11111));
    I = SInt::FromBits(0b1111);
    UInt U = IntCastOrZero<UInt>(I);
    exi_assert(U == 0);
    U = UInt::FromBits(I);

    outs() << "I: " << I << '\n';
    outs() << "U: " << U << '\n';
  } {
    using SInt = ibit<8>;
    using UInt = ubit<5>;

    SInt I = -1;
    exi_assert(CheckIntCast<SInt>(0b11111));
    UInt U = IntCastOrZero<UInt>(I);
    exi_assert(U == 0);

    I = SInt::FromBits(0b11111);
    exi_assert(I == 31);
    U = IntCastOrZero<UInt>(I);
    exi_assert(U == 31);
    U = UInt::FromBits(I);

    using i5 = ibit<5>;
    i5 I2 = i5::FromBits(U);
    i5 I3 = SInt(-1);
    exi_assert(I2 == -1);
    exi_assert(I2 == I3);

    outs() << "I: " << I << '\n';
    outs() << "U: " << U << '\n';
  }
}

template <typename...Args>
static void printWord(fmt::format_string<Args...> Fmt, Args&&...args) {
  if (gPrintAPWordBounds) {
    String Out = fmt::format(Fmt, EXI_FWD(args)...);
    fmt::print("[{}]", std::move(Out));
  } else {
    fmt::print(Fmt, EXI_FWD(args)...);
  }
}

static void printWordBits(u64 Block, unsigned Bits = 64) {
  using Arr = std::array<u8, sizeof(u64)>;
  auto A = std::bit_cast<Arr>(exi::byteswap(Block));
  unsigned Remaining = 64;
  for (u8 Byte : A) {
    if ((Remaining - 8) <= Bits) {
      if (Remaining > Bits)
        printWord("{: >08b}", Byte);
      else
        printWord("{:0>08b}", Byte);
    } else {
      printWord("        ");
    }
    fmt::print(" ");
    Remaining -= 8;
  }
}

static void printAPIntBinary(ArrayRef<u64> Arr, const char* Pre) {
  if (Pre && *Pre)
    fmt::print("{}: ", Pre);
  
  {
    // printWordBits(Arr.back(), Bits);
    printWordBits(Arr.back());
    Arr = Arr.drop_back();
    if (gPrintAPIntTail)
      fmt::print("| ");
  }

  if (!gPrintAPIntTail) {
    fmt::println("");
    return;
  }

  while (!Arr.empty()) {
    const u64 Curr = Arr.back();
    printWordBits(Arr.back());
    if (Arr.size() > 1)
      fmt::print("| ");
    Arr = Arr.drop_back();
  }

  fmt::println("");
}

static void printAPIntBinary(const APInt& AP, const char* Pre) {
  if (Pre && *Pre)
    fmt::print("{}: ", Pre);
  
  ArrayRef<u64> Arr = AP.getData();
  if (auto Bits = AP.getBitWidth() % 64; Bits != 0) {
    // printWordBits(Arr.back(), Bits);
    printWordBits(Arr.back());
    Arr = Arr.drop_back();
    if (gPrintAPIntTail)
      fmt::print("| ");
  }

  if (!gPrintAPIntTail) {
    fmt::println("");
    return;
  }

  while (!Arr.empty()) {
    const u64 Curr = Arr.back();
    printWordBits(Arr.back());
    if (Arr.size() > 1)
      fmt::print("| ");
    Arr = Arr.drop_back();
  }

  fmt::println("");
}

static void CompareAPInts(APInt& LHS, APInt RHS) {
  outs() << "AP:    " << LHS << '\n';
  outs() << "IS:    " << RHS << '\n';
  outs() << "APAPS: " << APSInt(LHS) << '\n';
  outs() << "BSAPS: " << APSInt(RHS) << '\n';
  outs() << '\n';

  if (LHS != RHS) {
    ::gPrintAPIntTail = false;
    ::gPrintAPWordBounds = true;
    printAPIntBinary(LHS, "AP");
    printAPIntBinary(RHS, "IS");
    outs() << '\n';
  }

#define TEST_BOTH(name, ...) do {                                              \
    auto Fn = [&](auto&& X) { return __VA_ARGS__; };                           \
    outs() << name << ": [" << Fn(LHS) << ", " << Fn(RHS) << "]\n";            \
  } while(0)

  TEST_BOTH("BitWidth    ", X.getBitWidth());
  TEST_BOTH("BitWidthAPS ", APSInt(X).getBitWidth());
  TEST_BOTH("popcount    ", X.popcount());
  TEST_BOTH("popcountAPS ", APSInt(X).popcount());
  TEST_BOTH("dataLength  ", X.getNumWords());
  outs() << '\n';
#undef TEST_BOTH
}

static void ReadAPIntExi(const APInt& AP, SmallVecImpl<u8>& Out) {
  SmallVec<u64> Vec(AP.getData());
  const usize Elts = Vec.size_in_bytes();
  std::reverse(Vec.begin(), Vec.end());

  u8* Raw = reinterpret_cast<u8*>(Vec.data());
  Out.assign(Raw, Raw + Elts);
}

#define DUMP_STMT_FMT(FMT, ...) fmt::println("{}: " FMT, #__VA_ARGS__, (__VA_ARGS__))
#define DUMP_STMT(...) DUMP_STMT_FMT("{}", __VA_ARGS__)

static void DumpU8Array(ArrayRef<u8> Bytes) {
  for (u8 Byte : Bytes) {
    const u8 Hi = (Byte >> 4);
    const u8 Lo = (Byte & 0xF);
    fmt::print("{:04b}'{:04b} ", Hi, Lo);
  }
  fmt::println("");
}

static void BitStreamTests(int Argc, char* Argv[]) noexcept {
  {
    u8 Data[4] {};
    bool Flag = false;
    auto DumpU8 = [Flag, &Data] {
      if (Flag) DumpU8Array(Data);
    };

    BitStreamWriter OS(Data);
    (void) OS.writeBits64(0b1001, 4);
    DumpU8();
    (void) OS.writeBits<3>(0b011);
    DumpU8();
    (void) OS.writeBit(0);
    DumpU8();
    (void) OS.writeBits64(0b1011, 4);
    DumpU8();
    (void) OS.writeBits64(0b1011'1111'1110, 12);
    DumpU8();
    (void) OS.writeBit(1);
    DumpU8();

    BitStreamReader IS(Data);
    exi_assert(IS.bitPos() == 0, "Yeah.");
    exi_assert(IS.peekBit()      == 1);
    exi_assert(IS.peekBits64(4)  == 0b1001);
    exi_assert(IS.readBits<4>()  == 0b1001);
    exi_assert(IS.readBits64(3)  == 0b011);
    exi_assert(IS.readBit()      == 0);
    exi_assert(IS.peekBits<4>()  == 0b1011);
    exi_assert(IS.readBits64(4)  == 0b1011);
    DUMP_STMT_FMT("{:#0b}", IS.peekBits64(12));
    exi_assert(IS.peekBits(12)   == 0b1011'1111'1110);
    exi_assert(IS.readBits64(12) == 0b1011'1111'1110);
    exi_assert(IS.readBit()      == 1);

    BitStreamWriter OS2(Data);
    BitStreamReader IS2(Data);
    BitStreamReader IS3(Data);

    IS2.setProxy(OS2.getProxy());
    IS3.setProxy(IS2.getProxy());
  } {
    u8 Data[6 * sizeof(u64)] {};
    bool MakeUnaligned = false;

    BitStreamWriter OS(Data);
    if (MakeUnaligned)
      (void) OS.writeBit(1);
    (void) OS.writeBits64(0b1001, 64);
    (void) OS.writeBits64(0x123456789ABCDEFull, 64);
    (void) OS.writeBits64((1ull << 63ull), 64);
    (void) OS.writeBits64(123, 63);
    (void) OS.writeBits<2>(0b10);
    (void) OS.writeBits<2>(0b01);

    {
      BitStreamReader IS(Data);
      auto printPos = [&IS] {
        fmt::print("{: ^4}: ", IS.bitPos());
      };

      if (MakeUnaligned) {
        printPos();
        DUMP_STMT(IS.readBit());
      }
      printPos();
      DUMP_STMT_FMT("{:#0b}", IS.readBits64(64));
      printPos();
      DUMP_STMT_FMT("{:#0x}", IS.readBits64(64));
      printPos();
      DUMP_STMT_FMT("{:#0b}", IS.readBits64(64));
      printPos();
      DUMP_STMT(IS.readBits64(63));
      printPos();
      DUMP_STMT_FMT("{:#02b}", IS.readBits<2>());
      printPos();
      DUMP_STMT_FMT("{:#02b}", IS.readBits<2>());
    }

    BitStreamReader IS(Data);
    if (MakeUnaligned)
      exi_assert(IS.readBit()    == 1);
    exi_assert(IS.readBits64(64) == 0b1001);
    exi_assert(IS.readBits64(64) == 0x123456789ABCDEF);
    exi_assert(IS.readBits64(64) == (1ull << 63ull));
    exi_assert(IS.readBits64(63) == 123);
  } SKIP {
    SmallVec<u64> Buf(5, 0x5F9C334508BB7DA4ull);
    constexpr usize kOff = 22;
    constexpr usize kBack = bitsizeof_v<u64> - kOff;
    const usize Bits = (Buf.size_in_bytes() * kCHAR_BIT) - kOff;

    SmallVec<u8> U8Data;
    U8Data.resize(Buf.capacity_in_bytes());
    // ReadAPIntExi(APInt(Bits, Buf), U8Data);
    {
      BitStreamWriter OS = BitStreamWriter::New(U8Data);
      (void) OS.writeBits(APInt(Bits, Buf));
    }

    auto GetNewStream = [kOff, &U8Data]() {
      BitStreamReader BSI(U8Data);
      // BSI.skip(kOff);
      return BSI;
    };

    BitStreamReader IS = GetNewStream();
    {
      OwningArrayRef<u8> U8Peek(U8Data.size());
      OwningArrayRef<u8> U8Read(U8Data.size());
      (void) IS.peek(U8Peek);
      (void) IS.read(U8Read);
      exi_assert(U8Data == U8Peek);
      exi_assert(U8Data == U8Read);
      IS = GetNewStream();
    }

    APInt AP(Bits, Buf);
    APInt BSAP = IS.peekBits(Bits);

    if (AP != BSAP) {
      {
        ArrayRef U8Arr(U8Data);
        BitStreamReader TIS(U8Arr.take_front(7));
        fmt::println("AP: {}\nIS: {}\n",
          AP.getData().back(),
          TIS.peekBits64(kBack));
      }
      CompareAPInts(AP, BSAP);
      printAPIntBinary(Buf, "SV");
    }
    exi_assert(IS.readBits(Bits) == AP);
  }


  // runAllTests();
  // return 0;
  BitIntTests();
}

#endif

//===----------------------------------------------------------------===//
// APInt
//===----------------------------------------------------------------===//

static void APIntTests(int, char*[]) noexcept {
  {
    SmallVec<u64> Buf(5, 0x5F9C334508BB7DA4ull);
    constexpr usize kOff = 22;
    constexpr usize kBack = bitsizeof_v<u64> - kOff;
    const usize Bits = (Buf.size_in_bytes() * kCHAR_BIT) - kOff;

    APInt Big(Bits, Buf);
    APInt Sml(22, 0x12345);
    exi_assert(Big != Sml);
  } {
    APInt Big(63, 0x12345);
    APInt Sml(22, 0x12345);
    exi_assert_eq(Big, Sml, "63:22");
  } {
    APInt Big(256, 0x12345);
    APInt Sml(22,  0x12345);
    exi_assert_eq(Big, Sml, "256:22");
  } {
    APInt Big(320, 0x12345);
    APInt Sml(192, 0x12345);
    exi_assert_eq(Big, Sml, "320:192");
  }
}

//===----------------------------------------------------------------===//
// ExiError
//===----------------------------------------------------------------===//

#define TEST_EE(FUNC, ...) (outs() << ExiError::FUNC(__VA_ARGS__) << '\n')

static void ExiErrorTests(int, char*[]) noexcept {
  TEST_EE(Full);
  TEST_EE(Header);
  TEST_EE(Mismatch);

  TEST_EE(Full, 0);
  TEST_EE(Full, 999);

  TEST_EE(HeaderSig, 'f');
  TEST_EE(HeaderBits, 0b00);
  TEST_EE(HeaderBits, 0b01);
  TEST_EE(HeaderBits, 0b10);
  TEST_EE(HeaderVer);
  TEST_EE(HeaderVer, 14);
  TEST_EE(HeaderAlign, AlignKind::BytePacked);
  TEST_EE(HeaderAlign, AlignKind::BitPacked, true);
  TEST_EE(HeaderStrict, PreserveKind::PIs);
  TEST_EE(HeaderStrict, PreserveKind::All);
  TEST_EE(HeaderSelfContained, AlignKind::PreCompression);
  TEST_EE(HeaderSelfContained, AlignKind::None);
  TEST_EE(HeaderSelfContained, AlignKind::None, true);
  TEST_EE(HeaderOutOfBand);
}

//===----------------------------------------------------------------===//
// Pointer*
//===----------------------------------------------------------------===//

static void PointerUnionTests(int, char*[]) noexcept {
  PointerUnion<ia8*, ia16*, ia32*, ia64*> AnyInt;
  {
    ia8 Val = 0;
    AnyInt = &Val;
    *cast<ia8*>(AnyInt) = 77;
    assert(Val == 77);
    AnyInt = nullptr; 
  }
  {
    ia32 Val = 44;
    AnyInt = &Val;
    assert(dyn_cast<ia8*>(AnyInt) == nullptr);
    *cast<ia32*>(AnyInt) = 44;
    assert(Val == 44);
    AnyInt = nullptr; 
  }
}

//===----------------------------------------------------------------===//
// Runes
//===----------------------------------------------------------------===//

static void CheckRuneCoding(StrRef UTF8, ArrayRef<char32_t> Expect) {
  if (Expect.back() == 0)
    Expect = Expect.drop_back();
  WithColor(errs()) << "Testing "
    << WithColor::BRIGHT_YELLOW << '"' << UTF8 << '"'
    << WithColor::RESET << ":\n";

  // Decode...
  SmallVec<Rune> Runes;
  if (!decodeRunes(UTF8, Runes)) {
    WithColor Save(errs(), WithColor::BRIGHT_RED);
    Save << "  error decoding string.\n\n";
    return;
  }

  if (Runes.size() != Expect.size()) {
    WithColor Save(errs(), WithColor::BRIGHT_RED);
    Save << "  size mismatch with expected.\n\n";
    return;
  }

  bool IsSame = true;
  for (i64 I = 0, E = Runes.size(); I != E; ++I) {
    const Rune ExRune = Rune(Expect[I]);
    if (Runes[I] == ExRune)
      continue;
    IsSame = false;
    errs() << "  mismatch at " << I << ": "
      << format("\\{:06x} -> \\{:06x}\n", Runes[I], ExRune);
  }

  if (!IsSame) {
    WithColor Save(errs(), WithColor::BRIGHT_RED);
    Save << "  decoding inconsistent.\n\n";
    return;
  }

  // Encode...
  SmallStr<64> Chars;
  if (!encodeRunes(Runes, Chars)) {
    WithColor Save(errs(), WithColor::BRIGHT_RED);
    Save << "  error encoding string.\n\n";
    return;
  }

  if (Chars.size() != UTF8.size()) {
    WithColor Save(errs(), WithColor::BRIGHT_RED);
    Save << "  size mismatch with utf8.\n\n";
    return;
  }

  IsSame = true;
  for (i64 I = 0, E = Chars.size(); I != E; ++I) {
    const char ExChar = UTF8[I];
    if (Chars[I] == ExChar)
      continue;
    IsSame = false;
    errs() << "  mismatch at " << I << ": "
      << format("\\{:06x} -> \\{:06x}\n", u8(Chars[I]), u8(ExChar));
  }

  if (!IsSame) {
    WithColor Save(errs(), WithColor::BRIGHT_RED);
    Save << "  encoding inconsistent.\n\n";
    return;
  }

  WithColor Save(errs(), WithColor::GREEN);
  Save << "  success!\n\n";
}

static void RuneTests(int, char*[]) {
  CheckRuneCoding(u8"Hello world, Καλημέρα κόσμε, コンニチハ",
                   U"Hello world, Καλημέρα κόσμε, コンニチハ");
  CheckRuneCoding(u8"∮ E⋅da = Q,  n → ∞, ∑ f(i) = ∏ g(i)",
                   U"∮ E⋅da = Q,  n → ∞, ∑ f(i) = ∏ g(i)");
  CheckRuneCoding(u8"∀x∈ℝ: ⌈x⌉ = −⌊−x⌋, α ∧ ¬β = ¬(¬α ∨ β)",
                   U"∀x∈ℝ: ⌈x⌉ = −⌊−x⌋, α ∧ ¬β = ¬(¬α ∨ β)");
  CheckRuneCoding(u8"ði ıntəˈnæʃənəl fəˈnɛtık əsoʊsiˈeıʃn",
                   U"ði ıntəˈnæʃənəl fəˈnɛtık əsoʊsiˈeıʃn");
  CheckRuneCoding(u8"((V⍳V)=⍳⍴V)/V←,V  ⌷←⍳→⍴∆∇⊃‾⍎⍕⌈",
                   U"((V⍳V)=⍳⍴V)/V←,V  ⌷←⍳→⍴∆∇⊃‾⍎⍕⌈");
  CheckRuneCoding(u8"კონფერენციაზე დასასწრებად, რომელიც გაიმართება",
                   U"კონფერენციაზე დასასწრებად, რომელიც გაიმართება");
  CheckRuneCoding(u8"๏ แผ่นดินฮั่นเสื่อมโทรมแสนสังเวช  พระปกเกศกองบู๊กู้ขึ้นใหม่",
                   U"๏ แผ่นดินฮั่นเสื่อมโทรมแสนสังเวช  พระปกเกศกองบู๊กู้ขึ้นใหม่");
  CheckRuneCoding(u8"ሰው እንደቤቱ እንጅ እንደ ጉረቤቱ አይተዳደርም።",
                   U"ሰው እንደቤቱ እንጅ እንደ ጉረቤቱ አይተዳደርም።");
  CheckRuneCoding(u8"ᚻᛖ ᚳᚹᚫᚦ ᚦᚫᛏ ᚻᛖ ᛒᚢᛞᛖ ᚩᚾ ᚦᚫᛗ ᛚᚪᚾᛞᛖ ᚾᚩᚱᚦᚹᛖᚪᚱᛞᚢᛗ ᚹᛁᚦ ᚦᚪ ᚹᛖᛥᚫ",
                   U"ᚻᛖ ᚳᚹᚫᚦ ᚦᚫᛏ ᚻᛖ ᛒᚢᛞᛖ ᚩᚾ ᚦᚫᛗ ᛚᚪᚾᛞᛖ ᚾᚩᚱᚦᚹᛖᚪᚱᛞᚢᛗ ᚹᛁᚦ ᚦᚪ ᚹᛖᛥᚫ");
}

//===----------------------------------------------------------------===//
// Bounded
//===----------------------------------------------------------------===//

static void BoundedTests(int, char*[]) noexcept {
  using T = u32;
  using Ty = Bounded<T>;
  Ty Val(77), Inf(exi::unbounded);

  exi_assert(Val == T(77));
  exi_assert(Val == 77);
  exi_assert(Val == Ty(77));
  exi_assert(Val != exi::unbounded);
  exi_assert(Val != Ty::Unbounded());

  exi_assert(Inf != T(77));
  exi_assert(Inf != 77);
  exi_assert(Inf != Ty(77));
  exi_assert(Inf == exi::unbounded);
  exi_assert(Inf == Ty::Unbounded());

  exi_assert(Val == Val);
  exi_assert(Inf == Inf);
  exi_assert(Val != Inf);
  exi_assert(Inf != Val);
  exi_assert(Val < Inf);
  exi_assert(Inf > Val);

  using Uy = Bounded<i32>;

  exi_assert(Val == Uy(77));
  exi_assert(Inf == Uy::Unbounded());
  exi_assert(Val != Uy::Unbounded());
  exi_assert(Inf != Uy(77));
  exi_assert(Val < Uy::Unbounded());
  exi_assert(Uy::Unbounded() > Val);

  exi_assert(exi::unbounded == exi::unbounded);
  exi_assert(exi::unbounded == Uy::Unbounded());
  exi_assert(Uy::Unbounded() == exi::unbounded);
  exi_assert(Ty::Unbounded() == Uy::Unbounded());
}

//===----------------------------------------------------------------===//
// Bounded
//===----------------------------------------------------------------===//

static void MaybeBoxTests(int, char*[]) noexcept {
  MaybeBox<String> MBox;
  auto Data = [&MBox] { return MBox.dataAndOwned(); };

  String Stk = "...";
  Option<String&> Opt(Stk);
  Box<String> Bx = std::make_unique<String>("..?");
  Naked<String> Nkd(Bx.get());

  exi_assert((Data() == std::pair{nullptr, false}));
  MBox = Stk;
  exi_assert((Data() == std::pair{&Stk, false}));
  MBox = Opt;
  exi_assert((Data() == std::pair{&Stk, false}));
  MBox = Nkd;
  exi_assert((Data() == std::pair{Bx.get(), false}));
  MBox = std::move(Bx);
  exi_assert((Data() == std::pair{Nkd.get(), true}));
}

//===----------------------------------------------------------------===//
// Poly
//===----------------------------------------------------------------===//

namespace poly_tests {

struct MyBase {
  void saySomething() const { std::printf("Center!\n"); }
};

struct Left : public MyBase {
  void saySomething() const { std::printf("Left!\n"); }
};

struct Right : public MyBase {
  void saySomething() const { std::printf("Right!\n"); }
};

using SPoly = Poly<MyBase, Left, Right>;

struct MyVBase {
  virtual ~MyVBase() {}
  virtual void saySomething() const = 0;
};

struct Meower : public MyVBase {
  void saySomething() const override { std::printf("Meow!\n"); }
};

struct Woofer : public MyVBase {
  void saySomething() const override { std::printf("Woof!\n"); }
};

using VPoly = Poly<MyVBase, Meower, Woofer>;

static void SaySomething(const SPoly& Val) {
  Val.visit([](auto& Val) { Val.saySomething(); });
}

static void SPolyTests() {
  SPoly x;
  exi_assert(x.empty());
  x = MyBase();
  SaySomething(x);
  x = Left();
  exi_assert(isa<Left>(x));
  if (auto X = dyn_cast<Left>(&x))
    X->saySomething();
  
  SPoly y = x;
  exi_assert(!y.empty());
  Right M {};
  y = M;
  exi_assert(isa<Right>(y));
  SaySomething(y);

  SPoly z = std::move(x);
  exi_assert(isa<Left>(z) && x.empty());
  SaySomething(z);

  Option<SPoly> O;
  exi_assert(!O.has_value());
  O = MyBase();
  SaySomething(*O);
  exi_assert(isa<MyBase>(*O));
}

static void VPolyTests() {
  VPoly x;
  exi_assert(x.empty());
  x = Meower();
  x->saySomething();
  x = Woofer();
  x->saySomething();
  exi_assert(isa<Woofer>(x));
  
  VPoly y = x;
  exi_assert(!y.empty());
  Meower M {};
  y = M;
  exi_assert(isa<Meower>(y));
  y->saySomething();

  VPoly z = std::move(x);
  exi_assert(isa<Woofer>(z) && x.empty());
  z->saySomething();

  Option<VPoly> O;
  exi_assert(!O.has_value());
  O = Meower();
  O.value()->saySomething();

#if 0
  x = O->take();
  x->saySomething();
  z.swap(x);
  x->saySomething();

  x = z.take();
  exi_assert(z.empty());
  exi_assert(isa<Meower>(x));
  z = Woofer();
  std::swap(x, z);
  exi_assert(isa<Woofer>(x));
#endif
}

} // namespace poly_tests

static void PolyTests(int, char*[]) {
  poly_tests::SPolyTests();
  poly_tests::VPolyTests();
}

//===----------------------------------------------------------------===//
// Result
//===----------------------------------------------------------------===//

namespace result_tests {

static int Global = 0;
static constexpr Result<int&, int> TestCxprResult(int In) {
  if (In == 0)
    return Ok(Global);
  else
    return Err(In);
}

static constexpr auto TCR_true = TestCxprResult(0);
static constexpr auto TCR_false = TestCxprResult(7);

static_assert(TCR_true.is_ok());
static_assert(TCR_false.is_err());

struct Base {
  virtual ~Base() = default;
  virtual int f() const { return 0; }
};
struct Derived : public Base {
  int f() const override{ return 1; }
};

} // namespace result_tests

static void ResultTests(int, char*[]) {
  using namespace result_tests;
  /*Result<T, E>*/ {
    Result<int, float> X(0);
    exi_assert(X.is_ok());

    X.emplace_error(0.0f);
    exi_assert(X.is_err());

    float F = 7.0f;
    Result<float, int> Y(Err(F));
    exi_assert(Y.is_err());

    Result<String, int> Z("Hello!");
    exi_assert(Z.is_ok());

    Z.emplace_error(1);
    exi_assert(Z.is_err());

    Result<const char*, short> A("Hello world!");
    exi_assert(A.is_ok());

    Z = A;
    exi_assert(Z.is_ok());
    exi_assert(Z->ends_with("world!"));
  }
  /*Result<T&, E>*/ {
    int I = 0;
    Result<int&, float> X(I);
    exi_assert(X.is_ok());

    X.emplace_error(0.0f);
    exi_assert(X.is_err());

    X.emplace(I);
    exi_assert(&*X == &I);
    exi_assert(X.data() == &I);
    exi_assert(X.value() == I);
    exi_assert(&X.value() == &I);

    float F = 7.0f;
    Result<float&, int> Y(Err(F));
    exi_assert(Y.is_err());

    Derived D;
    Result<Base&, int> Z(D);
    exi_assert(Z->f() == 1);
  }
  /*Result<T, E&>*/ {
    float F = 7.0f;
    Result<int, float&> X(Err(F));
    exi_assert(X.is_err());

    X = Ok(0);
    exi_assert(X.is_ok());

    Derived D;
    Result<int, Derived&> Y(X.value());
    exi_assert(Y.is_ok());

    Y = Err(D);
    exi_assert(Y.is_err());

    Result<int, Base&> Z(0);
    exi_assert(Z.is_ok());

    Z = Y;
    exi_assert(Z.is_err());
    exi_assert(Z.error().f() == 1);
  }
  /*Result<T&, E&>*/ {
    int I = 0;
    float F = 7.0f;
    Result<int&, float&> X(I);
    exi_assert(X.is_ok());

    X = Err(F);
    exi_assert(X.is_err());

    Result<int&, float&> Y(X);
    exi_assert(Y.is_err());

    Y = Ok(I);
    exi_assert(Y.is_ok());

    X = Y;
    exi_assert(X.is_ok());

    Result<int, float> Z(I);
    exi_assert(X.is_ok());

    X = Z.ref();
    exi_assert(&X.value() == &Z.value());
  }
}

//===----------------------------------------------------------------===//
// ...
//===----------------------------------------------------------------===//

void root::tests_main(int Argc, char* Argv[]) {
  // miscTests(Argc, Argv);
  // ExiErrorTests(Argc, Argv);
  // BitStreamTests(Argc, Argv);
  // APIntTests(Argc, Argv);
  RuneTests(Argc, Argv);
  // BoundedTests(Argc, Argv);
  // MaybeBoxTests(Argc, Argv);
  // PolyTests(Argc, Argv);
  ResultTests(Argc, Argv);
}
