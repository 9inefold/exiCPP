//===- Common/MaybeBoxed.hpp ----------------------------------------===//
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
/// This file provides an interface for pointers which may be boxed.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Box.hpp>
#include <Common/Naked.hpp>
#include <Common/Option.hpp>
#include <Common/PointerIntPair.hpp>
#include <type_traits>

namespace exi {

/// This class is used when a pointer may or may not be 
template <typename T> class MaybeBoxed {
	static_assert(PointerLikeTypeTraits<T*>::NumLowBitsAvailable > 0);
public:
	using PackedT = PointerIntPair<T*, 1>;
private:
	PackedT Data;
public:
	constexpr MaybeBoxed() = default;
	MaybeBoxed(const MaybeBoxed&) = delete;
	MaybeBoxed(MaybeBoxed&& O) : Data(std::move(O.Data)) { O.clearData(); }

	constexpr MaybeBoxed(std::nullptr_t) : MaybeBoxed() {}
	MaybeBoxed(T* Ptr, bool Owned) : Data(Ptr, Owned) {}
	MaybeBoxed(Naked<T> Ptr) : Data(Ptr.get(), false) {}
	MaybeBoxed(T& Ref EXI_LIFETIMEBOUND) : Data(&Ref, false) {}
	MaybeBoxed(Box<T>&& Ptr) : Data(Ptr.release(), true) {}
	MaybeBoxed(Option<T&> Opt) : Data(Opt.data(), false) {}

	~MaybeBoxed() { this->deleteData(); }

	MaybeBoxed& operator=(const MaybeBoxed&) = delete;
	MaybeBoxed& operator=(MaybeBoxed&& O) {
		this->deleteData();
		this->Data = std::move(O.Data);
		O.clearData();
		return *this;
	}

	MaybeBoxed& operator=(std::nullptr_t) {
		this->reset();
		return *this;
	}

	MaybeBoxed& operator=(Naked<T> Ptr) {
		this->deleteData();
		this->Data.setPointerAndInt(Ptr.get(), false);
		return *this;
	}

	MaybeBoxed& operator=(T& Ref EXI_LIFETIMEBOUND) {
		this->deleteData();
		this->Data.setPointerAndInt(&Ref, false);
		return *this;
	}

	MaybeBoxed& operator=(Box<T>&& Ptr) {
		this->deleteData();
		this->Data.setPointerAndInt(Ptr.release(), true);
		return *this;
	}

	MaybeBoxed& operator=(Option<T&> Opt) {
		this->deleteData();
		this->Data.setPointerAndInt(Opt.data(), false);
		return *this;
	}

	bool owned() const { return Data.getInt(); }
	T* get() const { return Data.getPointer(); }
	T* data() const { return Data.getPointer(); }

	std::pair<T*, bool> dataAndOwned() {
		return {data(), owned()};
	}

	T* operator->() const {
		exi_assert(data(), "value is inactive!");
		return data();
	}
	T& operator*() const {
		exi_assert(data(), "value is inactive!");
		return *data();
	}

	void reset() {
		this->deleteData();
		this->clearData();
	}

private:
	ALWAYS_INLINE void deleteData() {
		if (owned())
			delete this->get();
	}
	ALWAYS_INLINE void clearData() {
		Data.setPointerAndInt(nullptr, false);
	}
};

} // namespace exi
