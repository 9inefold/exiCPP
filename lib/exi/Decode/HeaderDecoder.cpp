//===- exi/Basic/HeaderDecoder.cpp ----------------------------------===//
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
/// This file implements decoding of the EXI Header from a stream.
///
//===----------------------------------------------------------------===//

#include <exi/Decode/HeaderDecoder.hpp>
#include <core/Support/Casting.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/ExiHeader.hpp>
#include <exi/Basic/NBitInt.hpp>

#define DEBUG_TYPE "HeaderDecoder"

using namespace exi;

namespace {
/// Used to update the input stream position on exit.
/// Makes error handling simpler.
class RTTISetter {
  BitStreamReader& In;
  StreamReader& Out;
public:
  RTTISetter(BitStreamReader& In, StreamReader& Out) : In(In), Out(Out) {}
  ~RTTISetter();
};
} // namespace `anonymous`

static BitStreamReader GetReader(StreamReader& Strm) {
  if (auto* BitS = dyn_cast<BitStreamReader*>(Strm))
    return BitStreamReader(BitS->getProxy());
  else {
    /* TODO: When supported...
    auto* ByteS = cast<ByteStreamReader*>(Strm);
    return BitStreamReader(ByteS->getProxy());
    */
    exi_unreachable("use of ByteStreamReader is currently unsupported");
  }
}

static void SetReader(StreamReader& Strm, BitStreamReader& MyReader) {
  auto Proxy = MyReader.getProxy();
  if (auto* BitS = dyn_cast<BitStreamReader*>(Strm))
    BitS->setProxy(Proxy);
  else {
    /* TODO: When supported...
    cast<ByteStreamReader*>(Strm)->setProxy(Proxy);
    */
    exi_unreachable("use of ByteStreamReader is currently unsupported");
  }
}

RTTISetter::~RTTISetter() { SetReader(Out, In); }

static ExiError DecodeCookieAndBits(ExiHeader& Header, BitStreamReader* Strm) {
  // The [Distinguishing Bits].
  ubit<2> DistinguishingBits;

  exi_try(Strm->peekBits(DistinguishingBits));
  if (DistinguishingBits == 0b00) {
    // Consume [EXI Cookie], if possible.
    for (const char C : "$EXI"_str) {
      const char Read = char(Strm->readByte());
      if EXI_UNLIKELY(C != Read) {
        LOG_ERROR("invalid cookie byte at '{}'", C);
        return ExiError::HeaderSig(Read);
      }
    }

    Header.HasCookie = true;
    LOG_EXTRA("header has cookie.");
  }
  
  exi_try(Strm->readBits(DistinguishingBits));
  if (DistinguishingBits != 0b10) {
    // File does not start with a valid sequence.
    return ExiError::HeaderBits(DistinguishingBits);
  }

  return ExiError::OK;
}

[[maybe_unused]] static ExiError
 DecodePresenceBit(ExiHeader& Header, BitStreamReader* Strm) {
  safe_bool PresenceBit;
  exi_try(Strm->readBits(PresenceBit));

  if (!Header.Opts) {
    if (!PresenceBit)
      return ExiError::HeaderOutOfBand();
    // Create unique instance for Opts.
    Header.Opts = std::make_unique<ExiOptions>();
  }

  if (!PresenceBit) {
    Header.HasOptions = false;
    LOG_EXTRA("out of band options provided.");
  }

  return ExiError::OK;
}

static ExiError DecodeVersion(ExiHeader& Header, BitStreamReader* Strm) {
  safe_bool PreviewBit;
  ubit<4> VersionChunk;

  exi_try(Strm->readBits(PreviewBit));
  Header.IsPreviewVersion = bool(PreviewBit);

  u32 VersionOut = 1;
  do {
    exi_try(Strm->readBits(VersionChunk));
    VersionOut += u32(VersionChunk);
  } while (VersionChunk == 0b1111);

  Header.ExiVersion = VersionOut;
  return ExiError::OK;
}

ExiError exi::decodeExiHeader(ExiHeader& Header, StreamReader& In) {
  BitStreamReader Strm = GetReader(In);
  RTTISetter SetOnExit(Strm, In);

  safe_bool PresenceBit;
  exi_try(DecodeCookieAndBits(Header, &Strm));
  exi_try(Strm.readBits(PresenceBit));

  if (!Header.Opts) {
    if (!PresenceBit)
      return ExiError::HeaderOutOfBand();
    // Create unique instance for Opts.
    Header.Opts = std::make_unique<ExiOptions>();
  } else if (PresenceBit) {
    LOG_WARN("ignored (potential) out-of-band opts");
  }

  exi_try(DecodeVersion(Header, &Strm));
  if (Header.IsPreviewVersion)
    return ExiError::HeaderVer();
  else if (Header.ExiVersion > kCurrentExiVersion)
    return ExiError::HeaderVer(Header.ExiVersion);
  
  LOG_EXTRA("EXI version: {}", Header.ExiVersion);

  if (!PresenceBit)
    LOG_EXTRA("out of band options provided");
  else {
    // TODO: Decode options from file.
    exi_unreachable("options decoding unimplemented");
  }

  auto& Opts = *Header.Opts;
  if (Opts.Compression)
    // Data is packed the same with precompression as with compression.
    // We set the alignment to this so the processor acts as such.
    Opts.Alignment = AlignKind::PreCompression;

  if (Opts.byteAligned())
    // Skip [Padding Bits].
    Strm.align();

  return ExiError::OK;
}
