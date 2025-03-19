//===- exi/Basic/BodyDecoder.cpp ------------------------------------===//
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
#include <core/Common/DenseMap.hpp>
#include <core/Common/Option.hpp>
#include <core/Common/StringMap.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/ExiHeader.hpp>
#include <exi/Decode/HeaderDecoder.hpp>
#include <exi/Stream/StreamVariant.hpp>
#include <variant>

namespace exi {
namespace decoder_detail {

template <typename Ret>
struct HandleVisitResult {
  using type = Ret;
  using opt_type = Option<Ret>;
};

template <>
struct HandleVisitResult<void> {
  using type = void;
  using opt_type = void;
};

template <typename Fn>
struct VisitResult {
  using ret_type = std::invoke_result_t<Fn, bitstream::BitReader&>;
  using type = typename HandleVisitResult<ret_type>::type;
};

template <typename Fn>
using visit_result_t = typename VisitResult<Fn>::type;

} // namespace decoder_detail

class ExiDecoder {
  ArrayRef<u8> Buffer;
  ExiHeader Header;

public:
  using ReaderVariant = std::variant<std::monostate, 
    bitstream::BitReader, bitstream::ByteReader>;

private:
  StreamReader Reader;
  ReaderVariant InlineReader;

public:
  ExiDecoder() = default;

  template <typename F> auto visitStream(F&& Func)
   -> decoder_detail::visit_result_t<F> {
    exi_invariant(Reader, "Uninitialized reader!");
    if (isa<bitstream::BitReader*>(Reader))
      return visitImpl<bitstream::BitReader>(EXI_FWD(Func));
    else
      return visitImpl<bitstream::ByteReader>(EXI_FWD(Func));
  }

  friend ExiError decodeHeader(ExiDecoder&);
  ExiError decodeHeader() {
    return exi::decodeHeader(*this);
  }

private:
  template <class T, typename AnyT>
  T* setReader(BitConsumerProxy<AnyT> Proxy) {
    T& NewReader = InlineReader.emplace<T>(Proxy);
    this->Reader = &NewReader;
    return &NewReader;
  }

  template <class StrmT, typename F>
  decltype(auto) visitImpl(F&& Func) {
    using ResultT = decoder_detail::visit_result_t<F>;
    auto* const Strm = cast<StrmT*>(Reader);
    return ResultT(EXI_FWD(Func)(*Strm));
  }
};

} // namespace exi
