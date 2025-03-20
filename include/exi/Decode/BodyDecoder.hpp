//===- exi/Basic/BodyDecoder.hpp ------------------------------------===//
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
#include <exi/Decode/UnifyBuffer.hpp>
#include <exi/Stream/StreamVariant.hpp>
#include <variant>

namespace exi {
class raw_ostream;
class ExiDecoder;

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

namespace decode {

/// The variant holding the inline reader. May be changed in the future.
using ReaderVariant = std::variant<std::monostate, 
  bitstream::BitReader, bitstream::ByteReader>;

/// A wrapper around a reader variant. Currently only has 2 variants (+empty).
class ReaderHolder {
  friend class exi::ExiDecoder;
  StreamReader Reader;
  mutable ReaderVariant Inline;
public:
  ReaderHolder() = default;

  /// Passes a reference to the stream to the provided functor.
  template <typename F> auto operator()(F&& Func)
   -> decoder_detail::visit_result_t<F> {
    exi_invariant(Reader, "Uninitialized reader!");
    if (isa<bitstream::BitReader*>(Reader))
      return this->visit<bitstream::BitReader>(EXI_FWD(Func));
    // TODO: Enable the ByteReader overload once it is defined.
    // else
    //   return this->visit<bitstream::ByteReader>(EXI_FWD(Func));
    exi_unreachable("invalid stream type!");
  }

  explicit operator bool() const {
    return static_cast<bool>(Reader);
  }

private:
  template <class T, typename AnyT>
  T* set(BitConsumerProxy<AnyT> Proxy) {
    T& NewReader = Inline.emplace<T>(Proxy);
    this->Reader = &NewReader;
    return &NewReader;
  }

  template <class StrmT, typename F>
  decltype(auto) visit(F&& Func) {
    using ResultT = decoder_detail::visit_result_t<F>;
    auto* const Strm = cast<StrmT*>(Reader);
    return ResultT(EXI_FWD(Func)(*Strm));
  }
};

} // namespace decode

/// The EXI decoding processor.
class ExiDecoder {
  ExiHeader Header;
  decode::ReaderHolder Reader;

  Option<raw_ostream&> OS;

public:
  ExiDecoder() = default;

  ExiDecoder(UnifiedBuffer Buffer) : ExiDecoder() {
    if (ExiError E = decodeHeader(Buffer))
      this->diagnose(E);
  }

  /// Diagnoses errors in the current context.
  void diagnose(ExiError E) const;

  /// Decodes the header from the provided buffer.
  /// Defined in `HeaderDecoder.cpp`.
  ExiError decodeHeader(UnifiedBuffer Buffer);
};

} // namespace exi
