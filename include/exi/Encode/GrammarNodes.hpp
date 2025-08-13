//===- exi/Encode/GrammarNodes.hpp ----------------------------------===//
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
/// This file defines the nodes used in the encoder grammar FoldingSet.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/D/OptionStorage.hpp>
#include <core/Common/FoldingSet.hpp>
#include <core/Support/Casting.hpp>
#include <exi/Basic/EventCodes.hpp>

#define GNODE_TYPES(X)                                                        \
  X(SEAny) X(SEUri) X(SEQName)                                                \
  X(ATAny) X(ATUri) X(ATQName)                                                \
  X(CH)

namespace exi {
namespace encode {
/// Represents the first part of an event code.
using FirstLevelProd = u32;
inline constexpr FirstLevelProd kInvalidFLProd = ~FirstLevelProd(0);

/// Defines the GrammarNode types.
namespace gnode {

class SENode;
class ATNode;
class CHNode;

class SEAnyNode;
class SEUriNode;
class SEQNameNode;

class ATAnyNode;
class ATUriNode;
class ATQNameNode;

/// Eg. `FINAL_NODE(CHNode, BaseNode)`
#define FINAL_NODE(NAME, BASE)                                                \
private:                                                                      \
  template <class> friend class exi::OptionStorage;                           \
  ALWAYS_INLINE constexpr NAME(std::nullopt_t) : BASE() {                     \
    static_assert(std::is_final_v<NAME>);                                     \
  }

/// Eg. `THIRDLEVEL_NODE(AT, ATQName)`
#define THIRDLEVEL_NODE(GENERIC, SPECIFIC)                                    \
  static constexpr auto kTerm = EventTerm::SPECIFIC;                          \
  FINAL_NODE(SPECIFIC##Node, GENERIC##Node)                                   \
public:                                                                       \
  constexpr SPECIFIC##Node() : GENERIC##Node(kTerm) {}                        \
  static bool classof(const BaseNode* N) { return N->kind() == kTerm; }

/// The base for GrammarNodes. It is the only first-level node.
class BaseNode : public FoldingSetNode {
  enum : u32 { kIllegalKind = kInvalidTerm };
  u32 Kind = kIllegalKind;
protected:
  constexpr BaseNode() = default;
  constexpr BaseNode(EventTerm K) : Kind(u32(K)) {}
public:
  /// Dispatches `Profile` to the proper types.
  inline void Profile(FoldingSetNodeID& ID) const;
  /// Gets the current `EventTerm`.
  ALWAYS_INLINE constexpr EventTerm kind() const {
    return EventTerm(Kind);
  }
  constexpr bool isValid() const {
    return Kind != kIllegalKind;
  }
  constexpr bool isaSE() const {
    return Kind >= unsigned(EventTerm::SEAny)
        && Kind <= unsigned(EventTerm::SEQName);
  }
  constexpr bool isaAT() const {
    return Kind >= unsigned(EventTerm::ATAny)
        && Kind <= unsigned(EventTerm::ATQName);
  }
  constexpr bool isaCH() const {
    return Kind == unsigned(EventTerm::CH)
        || Kind == unsigned(EventTerm::CHExtern);
  }
};

// Second-level nodes:

class SENode : public BaseNode {
protected:
  constexpr SENode() = default;
  EXI_INLINE constexpr SENode(EventTerm K) : BaseNode(K) {
    exi_invariant(this->isaSE());
  }
public:
  /// Dispatches `Profile` to the proper types.
  inline void Profile(FoldingSetNodeID& ID) const;
  static bool classof(const BaseNode* N) { return N->isaSE(); }
};

class ATNode : public BaseNode {
protected:
  constexpr ATNode() = default;
  EXI_INLINE constexpr ATNode(EventTerm K) : BaseNode(K) {
    exi_invariant(this->isaAT());
  }
public:
  /// Dispatches `Profile` to the proper types.
  inline void Profile(FoldingSetNodeID& ID) const;
  static bool classof(const BaseNode* N) { return N->isaAT(); }
};

class CHNode final : public BaseNode {
  static constexpr auto kTerm = EventTerm::CHExtern;
  FINAL_NODE(CHNode, BaseNode)
public:
  constexpr CHNode() : BaseNode(kTerm) {}
  void Profile(FoldingSetNodeID& ID) const {
    ID.AddInteger(unsigned(kTerm));
  }
  static bool classof(const BaseNode* N) {
    return N->kind() == kTerm;
  }
};

// Third-level nodes:

class SEAnyNode final : public SENode {
  THIRDLEVEL_NODE(SE, SEAny)
public:
  void Profile(FoldingSetNodeID& ID) const {
    ID.AddInteger(unsigned(kTerm));
  }
};

/// Currently unsupported!
class SEUriNode final : public SENode {
  THIRDLEVEL_NODE(SE, SEUri)
public:
  void Profile(FoldingSetNodeID& ID) const {
    ID.AddInteger(unsigned(kTerm));
  }
};

class SEQNameNode final : public SENode {
  THIRDLEVEL_NODE(SE, SEQName)
public:
  void Profile(FoldingSetNodeID& ID) const {
    ID.AddInteger(unsigned(kTerm));
  }
};

class ATAnyNode final : public ATNode {
  THIRDLEVEL_NODE(AT, ATAny)
public:
  void Profile(FoldingSetNodeID& ID) const {
    ID.AddInteger(unsigned(kTerm));
  }
};

/// Currently unsupported!
class ATUriNode final : public ATNode {
  THIRDLEVEL_NODE(AT, ATUri)
public:
  void Profile(FoldingSetNodeID& ID) const {
    ID.AddInteger(unsigned(kTerm));
  }
};

class ATQNameNode final : public ATNode {
  THIRDLEVEL_NODE(AT, ATQName)
public:
  void Profile(FoldingSetNodeID& ID) const {
    ID.AddInteger(unsigned(kTerm));
  }
};

#undef THIRDLEVEL_NODE
#undef FINAL_NODE

inline void BaseNode::Profile(FoldingSetNodeID& ID) const {
  switch (this->kind()) {
#define GNODE_SWITCH(TY)                                                      \
  case EventTerm::TY:                                                         \
    return static_cast<const TY##Node*>(this)->Profile(ID);
  GNODE_TYPES(GNODE_SWITCH)
#undef GNODE_SWITCH
  default:
    exi_guardrail("Invalid BaseNode kind!");
  }
}

EXI_FLATTEN inline void SENode::Profile(FoldingSetNodeID& ID) const {
  switch (this->kind()) {
  case EventTerm::SEAny:
    return static_cast<const SEAnyNode*>(this)->Profile(ID);
  case EventTerm::SEUri:
    return static_cast<const SEUriNode*>(this)->Profile(ID);
  case EventTerm::SEQName:
    return static_cast<const SEQNameNode*>(this)->Profile(ID);
  default:
    exi_guardrail("Invalid SENode kind!");
  }
}

EXI_FLATTEN inline void ATNode::Profile(FoldingSetNodeID& ID) const {
  switch (this->kind()) {
  case EventTerm::ATAny:
    return static_cast<const ATAnyNode*>(this)->Profile(ID);
  case EventTerm::ATUri:
    return static_cast<const ATUriNode*>(this)->Profile(ID);
  case EventTerm::ATQName:
    return static_cast<const ATQNameNode*>(this)->Profile(ID);
  default:
    exi_guardrail("Invalid ATNode kind!");
  }
}

/// Checks if a type is derived from `BaseNode` and `final`.
template <typename GTy>
concept is_complete = std::derived_from<GTy, BaseNode>
                   && std::is_final_v<GTy>;

} // namespace gnode
} // namespace encode

/// Implement optional storage for Grammar Nodes.
template <class NodeT>
requires encode::gnode::is_complete<NodeT>
class OptionStorage<NodeT> {
  static_assert(std::is_trivially_destructible_v<NodeT>);
  using BaseNodeT = encode::gnode::BaseNode;
  NodeT Data;

public:
  constexpr OptionStorage() : Data(std::nullopt) {}
  constexpr OptionStorage(std::in_place_t, auto&&...Args)
      : Data(EXI_FWD(Args)...) {}
  
  constexpr OptionStorage(const OptionStorage& Other) : Data(Other.Data) {}
  constexpr OptionStorage(OptionStorage&& Other) : Data(std::move(Other.Data)) {}

  constexpr OptionStorage& operator=(const NodeT& V) {
    this->Data = V;
    return *this;
  }
  constexpr OptionStorage& operator=(NodeT&& V) {
    this->Data = std::move(V);
    return *this;
  }

  inline constexpr NodeT& emplace(auto&&...Args) {
    if (std::is_constant_evaluated())
      std::destroy_at(&Data);
    std::construct_at(&Data, EXI_FWD(Args)...);
    return Data;
  }

  EXI_INLINE constexpr void reset() {
    if (std::is_constant_evaluated())
      std::destroy_at(&Data);
    new (&Data) NodeT(std::nullopt);
  }

  EXI_OPTIONSTORAGE_IMPL_VALUE(constexpr, NodeT, Data)

  ALWAYS_INLINE constexpr bool has_value() const {
    return static_cast<const BaseNodeT&>(Data).isValid();
  }
};

} // namespace exi

#undef GNODE_TYPES
