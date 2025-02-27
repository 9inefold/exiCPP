//===- Support/Atomic.hpp -------------------------------------------===//
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
/// This file provides an interface for atomics that are only active when
/// multithreading is active.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Support/Threading.hpp>
#include <atomic>
#include <concepts>

// TODO: Finish impl

#undef ATOMIC_MEMBER
#if EXI_USE_THREADS
# define ATOMIC_MEMBER Synced
#else
# define ATOMIC_MEMBER Unsynced
#endif

namespace exi::sys {
namespace atomic_detail {

#if EXI_USE_THREADS
/// Threading active, wrapped in `std::atomic`.
template <typename T>
using AtomicType = std::atomic<T>;

/// Returns `std::atomic<T>::is_always_lock_free`.
template <typename T>
concept is_lock_free = std::atomic<T>::is_always_lock_free;
#else
/// Threading inactive, raw type.
template <typename T>
using AtomicType = T;

/// Threading inactive, always true.
template <typename T>
concept is_lock_free = true;
#endif

/// Default storage for `Atomic`, this is trivially destructible.
template <typename T,
	bool = std::is_trivially_destructible_v<T>>
union Storage {
	T Unsynced;
	std::atomic<T> Synced;
public:
	inline constexpr Storage() noexcept
			: ATOMIC_MEMBER() {}
	inline constexpr Storage(T Direct) noexcept
			: ATOMIC_MEMBER(std::move(Direct)) {}
};

/// Default storage for `Atomic`, this is NOT trivially destructible.
template <typename T>
union Storage<T, false> {
	T Unsynced;
	std::atomic<T> Synced;
public:
	inline constexpr Storage() noexcept
			: ATOMIC_MEMBER() {}
	inline constexpr Storage(T Direct) noexcept
			: ATOMIC_MEMBER(std::move(Direct)) {}
	
	inline constexpr ~Storage() {
		using ActiveT = AtomicType<T>;
		ATOMIC_MEMBER.~ActiveT();
	}
};

} // namespace atomic_detail

/// Interface type for atomics, behaviour changes depending on if threading
/// is enabled or not.
template <typename T> class Atomic {
	using ActiveT = atomic_detail::AtomicType<T>;
	atomic_detail::Storage<T> Storage;
public:
	static constexpr bool is_always_lock_free
		= atomic_detail::is_lock_free<T>;

	constexpr Atomic() noexcept = default;
	constexpr Atomic(T Direct) noexcept
			: Storage(std::move(Direct)) {}
	Atomic(const Atomic&) = delete;
};

} // namespace exi::sys

#undef ATOMIC_MEMBER
