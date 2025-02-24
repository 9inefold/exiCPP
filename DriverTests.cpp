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

#include <Common/AlignedInt.hpp>
#include <Common/APSInt.hpp>
#include <Common/Box.hpp>
#include <Common/Map.hpp>
#include <Common/PointerUnion.hpp>
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
#include <Support/MemoryBufferRef.hpp>
#include <Support/Process.hpp>
#include <Support/raw_ostream.hpp>
#include <fmt/format.h>
#include <fmt/chrono.h>
#include <fmt/ranges.h>

#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/ProcTypes.hpp>
#include <exi/Stream/BitStream.hpp>

#include <tuple>
#include <malloc.h>
#include <windows.h>

// TODO: Tests!!

using namespace exi;
using namespace exi::sys;

#undef  SKIP
#define SKIP if (0)

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

static void BitStreamTests(int Argc, char* Argv[]) noexcept {
  SKIP {
    u8 Data[4] {};

    BitStreamOut OS(Data);
    (void) OS.writeBits64(0b1001, 4);
    (void) OS.writeBits<3>(0b011);
    (void) OS.writeBit(0);
    (void) OS.writeBits64(0b1011, 4);
    (void) OS.writeBits64(0b1011'1111'1110, 12);
    (void) OS.writeBit(1);

    BitStreamIn IS(Data);
    exi_assert(IS.bitPos() == 0, "Yeah.");
    exi_assert(IS.peekBit()      == 1);
    exi_assert(IS.peekBits64(4)  == 0b1001);
    exi_assert(IS.readBits<4>()  == 0b1001);
    exi_assert(IS.readBits64(3)  == 0b011);
    exi_assert(IS.readBit()      == 0);
    exi_assert(IS.peekBits<4>()  == 0b1011);
    exi_assert(IS.readBits64(4)  == 0b1011);
    exi_assert(IS.peekBits(12)   == 0b1011'1111'1110);
    exi_assert(IS.readBits64(12) == 0b1011'1111'1110);
    exi_assert(IS.readBit()      == 1);
  } {
    u8 Data[6 * sizeof(u64)] {};
    bool MakeUnaligned = false;

    BitStreamOut OS(Data);
    if (MakeUnaligned)
      (void) OS.writeBit(1);
    (void) OS.writeBits64(0b1001, 64);
    (void) OS.writeBits64(0x123456789ABCDEFull, 64);
    (void) OS.writeBits64((1ull << 63ull), 64);
    (void) OS.writeBits64(123, 63);
    (void) OS.writeBits<2>(0b10);
    (void) OS.writeBits<2>(0b01);

    {
      BitStreamIn IS(Data);
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

    BitStreamIn IS(Data);
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
      BitStreamOut OS = BitStreamOut::New(U8Data);
      (void) OS.writeBits(APInt(Bits, Buf));
    }

    auto GetNewStream = [kOff, &U8Data]() {
      BitStreamIn BSI(U8Data);
      // BSI.skip(kOff);
      return BSI;
    };

    BitStreamIn IS = GetNewStream();
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
        BitStreamIn TIS(U8Arr.take_front(7));
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
  TEST_EE(HeaderStrict, Preserve::PIs);
  TEST_EE(HeaderStrict, Preserve::All);
  TEST_EE(HeaderSelfContained, AlignKind::PreCompression);
  TEST_EE(HeaderSelfContained, AlignKind::None);
  TEST_EE(HeaderSelfContained, AlignKind::None, true);
  TEST_EE(HeaderOutOfBand);
}

//===----------------------------------------------------------------===//
// Pointer*
//===----------------------------------------------------------------===//

void PointerUnionTests(int, char*[]) noexcept {
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

void tests_main(int Argc, char* Argv[]) {
  // miscTests(Argc, Argv);
  ExiErrorTests(Argc, Argv);
  BitStreamTests(Argc, Argv);
  APIntTests(Argc, Argv);
}
