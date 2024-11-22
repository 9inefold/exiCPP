//===- Errors.cpp ---------------------------------------------------===//
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

#include <Errors.hpp>
#include <memory>
#include <string>

using namespace exi;
using Traits = std::char_traits<char>;

Error& Error::operator=(Error&& e) {
  this->clear();
	this->msg = e.msg;
	this->state = e.state;
	e.clear();
	return *this;
}

Error Error::From(StrRef msg, bool clone) {
	if (!clone)
		return Error(msg.data());
	if (msg.empty())
		return Error("");
  return Error::MakeOwned(msg);
}

Error Error::From(ErrCode err) {
	if EXICPP_LIKELY(err == ErrCode::Ok)
		return Error::Ok();
	const auto msg = getErrString(err);
	return Error::From(msg);
}

Error Error::MakeOwned(StrRef msg) {
  const std::size_t size = msg.size();
	auto ptr = std::make_unique<char[]>(size + 1);
  Traits::copy(ptr.get(), msg.data(), size);
  return Error(ptr.release(), Error::Owned);
}

void Error::clear() {
  if (state == Error::Owned) {
    auto* mut_msg = const_cast<char*>(msg);
	  delete[] mut_msg;
  }
	this->msg = nullptr;
  this->state = Error::Invalid;
}

StrRef Error::message() const {
	if (this->state == NoError)
		return "Success";
	return {this->msg, strsize(this->msg)};
}
