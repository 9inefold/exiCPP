//===- Common/OpaqueHandle.hpp --------------------------------------===//
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
/// This file defines a simple interface for creating type-safe opaque handles.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <concepts>
#include <type_traits>

namespace exi {

template <typename...Features>
struct OpaqueFeatures;

namespace H {
struct OpaqueFeaturesTag;

template <typename T>
concept has_features_tag = std::derived_from<T, OpaqueFeaturesTag>;
} // namespace H

template <class T>
struct OpaqueFeaturesInfo {
  COMPILE_FAILURE(OpaqueFeatures,
    "Invalid opaque features!")
};

template <typename...Features>
struct OpaqueFeaturesInfo<OpaqueFeatures<Features...>> {
  using type = OpaqueFeatures<Features...>;
  using real_type = OpaqueFeatures<Features...>;
  static constexpr usize size = sizeof...(Features);
  // TODO: Implement features?
};

template <>
struct OpaqueFeaturesInfo<void> {
  using type = OpaqueFeatures<>;
  using real_type = void;
  static constexpr usize size = 0;
};

template <H::has_features_tag Remap>
struct OpaqueFeaturesInfo<Remap>
    : OpaqueFeaturesInfo<typename Remap::features> {
  using real_type = Remap;
};

//===----------------------------------------------------------------===//
// OpaqueHandle
//===----------------------------------------------------------------===//

template <class Name>
concept valid_handle_name = !std::is_void_v<Name>;

/// A proxy type which allows opaque handles to have their information queried.
/// Simplifies some logic on the user's end.
/// @tparam Name A unique type that identifies the handle.
/// @tparam Group A type which can further disambiguate handles.
/// @tparam Features Currently unused.
template <
  valid_handle_name Name,
  class Group = void,
  class Features = OpaqueFeatures<>>
struct OpaqueHandle;

/// Defines an opaque handle type without a group.
/// @tparam Name A unique type that identifies the handle.
template <
  valid_handle_name Name,
  class Features = OpaqueFeatures<>>
using GrouplessOpaqueHandle = OpaqueHandle<Name, void, Features>;

/// Defines an opaque handle type with the group in front. Used for macros.
template <
  class Group,
  valid_handle_name Name,
  class Features = OpaqueFeatures<>>
using GroupFirstOpaqueHandle = OpaqueHandle<Name, Group, Features>;

//===----------------------------------------------------------------===//
// Traits
//===----------------------------------------------------------------===//

template <class T> struct OpaqueHandleInfo {
  static constexpr bool is_handle = false;
  using name = void;
  using group = void;
  using features = void;
};

template <class Name, class Group, class Features>
struct OpaqueHandleInfo<OpaqueHandle<Name, Group, Features>> {
  static constexpr bool is_handle = true;
  using name_t = Name;
  using group_t = Group;
  using features_t = Features;
};

//////////////////////////////////////////////////////////////////////////
// Accessors

/// Gets the "name" of a handle.
template <class Handle>
using handle_name_t = typename OpaqueHandleInfo<Handle>::name_t;

/// Gets the group of a handle.
template <class Handle>
using handle_group_t = typename OpaqueHandleInfo<Handle>::group_t;

/// Gets the features of a handle.
template <class Handle>
using handle_features_t = typename OpaqueHandleInfo<Handle>::features_t;

template <class Handle>
using features_info_t = OpaqueFeaturesInfo<handle_features_t<Handle>>;

//////////////////////////////////////////////////////////////////////////
// Checks

/// Checks if the type `T` is an opaque handle.
template <class Handle>
concept is_opaque_handle = OpaqueHandleInfo<Handle>::is_handle;

/// Checks if the handle is not in a group.
template <class Handle>
concept is_groupless_handle = std::same_as<void, handle_group_t<Handle>>;

/// Checks if the handle is in a group.
template <class Handle>
concept is_grouped_handle = !is_groupless_handle<Handle>;

/// Checks if the handle is in a specific group.
template <class Handle, class Group>
concept handle_in_group = std::same_as<Group, handle_group_t<Handle>>;

/// Checks if the handle has no features.
template <class Handle>
concept is_featureless_handle
  = features_info_t<Handle>::size == 0;

/// Checks if the handle has features.
template <class Handle>
concept is_featured_handle
  = !is_featureless_handle<Handle>;

//===----------------------------------------------------------------===//
// Macros
//===----------------------------------------------------------------===//

/// Required for creating a feature remapping.
#define EXI_FEATURES_DECL(NAME) NAME : ::exi::H::OpaqueFeaturesTag
/// Defines a feature mapping to shrink symbol sizes.
#define EXI_FEATURES_REMAP(NAME, ...)                                         \
struct EXI_FEATURES_DECL(NAME) {                                              \
  using features = ::exi::OpaqueFeatures<__VA_ARGS__>;                        \
};

/// Defines a handle's "real name".
#define EXI_OPAQUE_NAME(NAME) struct _h##NAME
/// Defines a grouped handle type.
#define EXI_OPAQUE_HANDLE_T(NAME, ...)                                        \
  ::exi::OpaqueHandle<EXI_OPAQUE_NAME(NAME) __VA_OPT__(,) __VA_ARGS__>
/// Defines a groupless handle type.
#define EXI_GROUPLESS_HANDLE_T(NAME, ...)                                     \
  ::exi::GrouplessOpaqueHandle<EXI_OPAQUE_NAME(NAME) __VA_OPT__(,) __VA_ARGS__>
/// Defines a group-first handle type.
#define EXI_GROUPFIRST_HANDLE_T(GROUP, NAME, ...)                             \
  EXI_OPAQUE_HANDLE_T(NAME, GROUP __VA_OPT__(,) __VA_ARGS__)

/// Defines a grouped handle type.
#define EXI_OPAQUE_HANDLE(NAME, ...)                                          \
  using NAME = EXI_OPAQUE_HANDLE_T(NAME, __VA_ARGS__);
/// Defines a groupless handle type.
#define EXI_GROUPLESS_HANDLE(NAME, ...)                                       \
  using NAME = EXI_GROUPLESS_HANDLE_T(NAME, __VA_ARGS__);

} // namespace exi
