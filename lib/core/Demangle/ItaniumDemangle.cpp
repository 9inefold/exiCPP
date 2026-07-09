//===------------------------- ItaniumDemangle.cpp ------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// FIXME: (possibly) incomplete list of features that clang mangles that this
// file does not yet support:
//   - C++ modules TS

#include <core/Demangle/Demangle.hpp>
#include <core/Demangle/ItaniumDemangle.hpp>
#include <core/Common/SmallPtrSet.hpp>
#include <core/Common/SmallVec.hpp>
#include <core/Common/STLExtras.hpp>
#include <core/Support/Alloc.hpp>
#include <core/Support/IntCast.hpp>
#include <core/Support/ScopedSave.hpp>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <utility>
//#include <vector>

CLANG_IGNORED("-Wmissing-field-initializers")

using namespace exi;
using namespace exi::itanium_demangle;

// <discriminator> := _ <non-negative number>      # when number < 10
//                 := __ <non-negative number> _   # when number >= 10
//  extension      := decimal-digit+               # at the end of string
const char *itanium_demangle::parse_discriminator(const char *first,
                                                  const char *last) {
  // parse but ignore discriminator
  if (first != last) {
    if (*first == '_') {
      const char *t1 = first + 1;
      if (t1 != last) {
        if (std::isdigit(*t1))
          first = t1 + 1;
        else if (*t1 == '_') {
          for (++t1; t1 != last && std::isdigit(*t1); ++t1)
            ;
          if (t1 != last && *t1 == '_')
            first = t1 + 1;
        }
      }
    } else if (std::isdigit(*first)) {
      const char *t1 = first + 1;
      for (; t1 != last && std::isdigit(*t1); ++t1)
        ;
      if (t1 == last)
        first = last;
    }
  }
  return first;
}

namespace {

template <typename NodeT>
concept itanium_node = std::derived_from<NodeT, itanium_demangle::Node>;

struct BaseVisitor {
  constexpr BaseVisitor() = default;

  EXI_NO_INLINE static usize node_size(const itanium_demangle::Node* N) {
    DEMANGLE_ASSERT(N, "Node cannot be null!");
    switch (N->getKind()) {
#define NODE(X)                                                                \
    case Node::K##X:                                                           \
      return sizeof(itanium_demangle::X);
#include <core/Demangle/ItaniumNodes.mac>
    case Node::KNodeProxyNode:
      return sizeof(itanium_demangle::NodeProxyNode);
    }
    DEMANGLE_ASSERT(0, "unknown mangling node kind");
    DEMANGLE_UNREACHABLE;
  }
  template <itanium_node NodeT>
  static constexpr usize node_size(const NodeT* N) {
    return sizeof(NodeT);
  }

  EXI_NO_INLINE static const char* node_name(const itanium_demangle::Node* N) {
    DEMANGLE_ASSERT(N, "Node cannot be null!");
    switch (N->getKind()) {
#define NODE(X)                                                                \
    case Node::K##X:                                                           \
      return #X;
#include <core/Demangle/ItaniumNodes.mac>
    case Node::KNodeProxyNode:
      return "NodeProxyNode";
    }
    DEMANGLE_ASSERT(0, "unknown mangling node kind");
    DEMANGLE_UNREACHABLE;
  }
  template <itanium_node NodeT>
  static constexpr const char* node_name(const NodeT*) {
    return NodeKind<NodeT>::name();
  }

  template <itanium_node To>
  static constexpr bool node_isa(const itanium_demangle::Node& N) {
    return N.getKind() == itanium_demangle::NodeKind<To>::Kind;
  }
  template <class To>
  ALWAYS_INLINE static constexpr bool node_isa(const itanium_demangle::Node* N) {
    DEMANGLE_ASSERT(N, "Node cannot be null!");
    return node_isa<To>(*N);
  }

  template <itanium_node To1, itanium_node To2, itanium_node...ToRest>
  EXI_FLATTEN static constexpr bool node_isa(const itanium_demangle::Node& N) {
    return node_isa<To1>(N) || node_isa<To2, ToRest...>(N);
  }
  template <itanium_node To1, itanium_node To2, itanium_node...ToRest>
  ALWAYS_INLINE static constexpr bool node_isa(const itanium_demangle::Node* N) {
    DEMANGLE_ASSERT(N, "Node cannot be null!");
    return node_isa<To1, To2, ToRest...>(*N);
  }

  template <itanium_node To>
  static constexpr To& node_cast(itanium_demangle::Node& N) {
    DEMANGLE_ASSERT(node_isa<To>(N), "Invalid node type!");
    return *static_cast<To*>(&N);
  }
  template <itanium_node To>
  static constexpr const To& node_cast(const itanium_demangle::Node& N) {
    DEMANGLE_ASSERT(node_isa<To>(N), "Invalid node type!");
    return *static_cast<const To*>(&N);
  }
  template <itanium_node To>
  static constexpr To* node_cast(itanium_demangle::Node* N) {
    if (node_isa<To>(N))
      return static_cast<To*>(N);
    return nullptr;
  }
  template <itanium_node To>
  static constexpr const To* node_cast(const itanium_demangle::Node* N) {
    if (node_isa<To>(N))
      return static_cast<const To*>(N);
    return nullptr;
  }

  template <itanium_node To>
  static constexpr To* node_cast_if_present(itanium_demangle::Node* N) {
    if (N == nullptr)
      return nullptr;
    return &node_cast<To>(*N);
  }
  template <itanium_node To>
  static constexpr const To* node_cast_if_present(const itanium_demangle::Node* N) {
    if (N == nullptr)
      return nullptr;
    return &node_cast<To>(*N);
  }
};

struct BasePrintVisitor : public BaseVisitor {
  constexpr BasePrintVisitor() = default;

  using BaseVisitor::node_isa;
  using BaseVisitor::node_cast;
  using BaseVisitor::node_cast_if_present;

  void printStr(const char *S) const { fprintf(stderr, "%s", S); }
  template <bool Quote = false>
  void printStr(const char *S, auto Size) const {
    const int IntSize = IntCastOr<int>(Size, -1);
    if EXI_ALWAYS(IntSize > 0) {
      if constexpr (!Quote)
        fprintf(stderr, "%.*s", IntSize, S);
      else
        fprintf(stderr, "\"%.*s\"", IntSize, S);
    }
  }
  ALWAYS_INLINE void print(std::string_view SV) const {
    printStr</*Quote=*/true>(SV.data(), SV.size());
  }

  template <char C>
  EXI_NO_INLINE void printPadding(unsigned NumChars) const {
    static const char Chars[] = {C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,
                                 C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,
                                 C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,
                                 C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,
                                 C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C};

    // Usually the indentation is small, handle it with a fastpath.
    if EXI_LIKELY(NumChars < std::size(Chars)) {
      printStr(Chars, NumChars);
      return;
    }

    while (NumChars) {
      unsigned NumToWrite = std::min(NumChars, unsigned(std::size(Chars)) - 1u);
      printStr(Chars, NumToWrite);
      NumChars -= NumToWrite;
    }
  }

  // Overload used when T is exactly 'bool', not merely convertible to 'bool'.
  void print(bool B) { printStr(B ? "true" : "false"); }

  template <class T> std::enable_if_t<std::is_unsigned<T>::value> print(T N) {
    fprintf(stderr, "%llu", (unsigned long long)N);
  }

  template <class T> std::enable_if_t<std::is_signed<T>::value> print(T N) {
    fprintf(stderr, "%lld", (long long)N);
  }
};
} // namespace `anonymous`

#ifndef NDEBUG
namespace {
struct DumpVisitor : public BasePrintVisitor {
  unsigned Depth = 0;
  bool PendingNewline = false;

  template <typename NodeT>
  static constexpr bool wantsNewline(const NodeT *) { return true; }
  static bool wantsNewline(NodeArray A) { return !A.empty(); }
  static constexpr bool wantsNewline(...) { return false; }

  template <typename...Ts> static bool anyWantNewline(Ts...Vs) {
    return (wantsNewline(Vs) || ...);
  }

  using BasePrintVisitor::printStr;
  using BasePrintVisitor::print;
  using BasePrintVisitor::printPadding;

  void print(const Node *N) {
    if (N)
      N->visit(std::ref(*this));
    else
      printStr("<null>");
  }
  void print(NodeArray A) {
    ++Depth;
    printStr("{");
    bool First = true;
    for (const Node *N : A) {
      if (First)
        print(N);
      else
        printWithComma(N);
      First = false;
    }
    printStr("}");
    --Depth;
  }

  void print(ReferenceKind RK) {
    switch (RK) {
    case ReferenceKind::LValue:
      return printStr("ReferenceKind::LValue");
    case ReferenceKind::RValue:
      return printStr("ReferenceKind::RValue");
    }
  }
  void print(FunctionRefQual RQ) {
    switch (RQ) {
    case FunctionRefQual::FrefQualNone:
      return printStr("FunctionRefQual::FrefQualNone");
    case FunctionRefQual::FrefQualLValue:
      return printStr("FunctionRefQual::FrefQualLValue");
    case FunctionRefQual::FrefQualRValue:
      return printStr("FunctionRefQual::FrefQualRValue");
    }
  }
  void print(Qualifiers Qs) {
    if (!Qs) return printStr("QualNone");
    struct QualName { Qualifiers Q; const char *Name; } Names[] = {
      {QualConst, "QualConst"},
      {QualVolatile, "QualVolatile"},
      {QualRestrict, "QualRestrict"},
    };
    for (QualName Name : Names) {
      if (Qs & Name.Q) {
        printStr(Name.Name);
        Qs = Qualifiers(Qs & ~Name.Q);
        if (Qs) printStr(" | ");
      }
    }
  }
  void print(FundamentalTypeKind FTK) {
    switch (FTK) {
    case FundamentalTypeKind::Integer:
      return printStr("FundamentalTypeKind::Integer");
    case FundamentalTypeKind::Float:
      return printStr("FundamentalTypeKind::Float");
    case FundamentalTypeKind::Decimal:
      return printStr("FundamentalTypeKind::Decimal");
    case FundamentalTypeKind::Accum:
      return printStr("FundamentalTypeKind::Accum");
    case FundamentalTypeKind::Fract:
      return printStr("FundamentalTypeKind::Fract");
    case FundamentalTypeKind::SatAccum:
      return printStr("FundamentalTypeKind::SatAccum");
    case FundamentalTypeKind::SatFract:
      return printStr("FundamentalTypeKind::SatFract");
    }
  }
  void print(SpecialSubKind SSK) {
    switch (SSK) {
    case SpecialSubKind::allocator:
      return printStr("SpecialSubKind::allocator");
    case SpecialSubKind::basic_string:
      return printStr("SpecialSubKind::basic_string");
    case SpecialSubKind::string:
      return printStr("SpecialSubKind::string");
    case SpecialSubKind::istream:
      return printStr("SpecialSubKind::istream");
    case SpecialSubKind::ostream:
      return printStr("SpecialSubKind::ostream");
    case SpecialSubKind::iostream:
      return printStr("SpecialSubKind::iostream");
    }
  }
  void print(TemplateParamKind TPK) {
    switch (TPK) {
    case TemplateParamKind::Type:
      return printStr("TemplateParamKind::Type");
    case TemplateParamKind::NonType:
      return printStr("TemplateParamKind::NonType");
    case TemplateParamKind::Template:
      return printStr("TemplateParamKind::Template");
    }
  }
  void print(Node::Prec P) {
    switch (P) {
    case Node::Prec::Primary:
      return printStr("Node::Prec::Primary");
    case Node::Prec::Postfix:
      return printStr("Node::Prec::Postfix");
    case Node::Prec::Unary:
      return printStr("Node::Prec::Unary");
    case Node::Prec::Cast:
      return printStr("Node::Prec::Cast");
    case Node::Prec::PtrMem:
      return printStr("Node::Prec::PtrMem");
    case Node::Prec::Multiplicative:
      return printStr("Node::Prec::Multiplicative");
    case Node::Prec::Additive:
      return printStr("Node::Prec::Additive");
    case Node::Prec::Shift:
      return printStr("Node::Prec::Shift");
    case Node::Prec::Spaceship:
      return printStr("Node::Prec::Spaceship");
    case Node::Prec::Relational:
      return printStr("Node::Prec::Relational");
    case Node::Prec::Equality:
      return printStr("Node::Prec::Equality");
    case Node::Prec::And:
      return printStr("Node::Prec::And");
    case Node::Prec::Xor:
      return printStr("Node::Prec::Xor");
    case Node::Prec::Ior:
      return printStr("Node::Prec::Ior");
    case Node::Prec::AndIf:
      return printStr("Node::Prec::AndIf");
    case Node::Prec::OrIf:
      return printStr("Node::Prec::OrIf");
    case Node::Prec::Conditional:
      return printStr("Node::Prec::Conditional");
    case Node::Prec::Assign:
      return printStr("Node::Prec::Assign");
    case Node::Prec::Comma:
      return printStr("Node::Prec::Comma");
    case Node::Prec::Default:
      return printStr("Node::Prec::Default");
    }
  }

  void newLine() {
    printStr("\n");
    if EXI_LIKELY(Depth > 0)
      printPadding<' '>(Depth);
    PendingNewline = false;
  }

  template<typename T> void printWithPendingNewline(T V) {
    print(V);
    if (wantsNewline(V))
      PendingNewline = true;
  }

  template<typename T> void printWithComma(T V) {
    if (PendingNewline || wantsNewline(V)) {
      printStr(",");
      newLine();
    } else {
      printStr(", ");
    }

    printWithPendingNewline(V);
  }

  struct CtorArgPrinter {
    DumpVisitor &Visitor;

    template<typename T, typename ...Rest> void operator()(T V, Rest ...Vs) {
      if (Visitor.anyWantNewline(V, Vs...))
        Visitor.newLine();
      Visitor.printWithPendingNewline(V);
      int PrintInOrder[] = { (Visitor.printWithComma(Vs), 0)..., 0 };
      (void)PrintInOrder;
    }
  };

  template<typename NodeT> void operator()(const NodeT *Node) {
    Depth += 2;
    fprintf(stderr, "%s(", itanium_demangle::NodeKind<NodeT>::name());
    Node->match(CtorArgPrinter{*this});
    fprintf(stderr, ")");
    Depth -= 2;
  }

  void operator()(const ForwardTemplateReference *Node) {
    Depth += 2;
    fprintf(stderr, "ForwardTemplateReference(");
    if (Node->Ref && !Node->Printing) {
      Node->Printing = true;
      CtorArgPrinter{*this}(Node->Ref);
      Node->Printing = false;
    } else {
      CtorArgPrinter{*this}(Node->Index);
    }
    fprintf(stderr, ")");
    Depth -= 2;
  }
};
} // namespace `anonymous`

void itanium_demangle::Node::dump() const {
  DumpVisitor V;
  visit(std::ref(V));
  V.newLine();
}
#endif

namespace {
class BumpPointerAllocator {
  struct BlockMeta {
    BlockMeta* Next;
    size_t Current;
  };

  static constexpr size_t AllocSize = 4096;
  static constexpr size_t UsableAllocSize = AllocSize - sizeof(BlockMeta);

  alignas(long double) char InitialBuffer[AllocSize];
  BlockMeta* BlockList = nullptr;

  void grow() {
    char* NewMeta = static_cast<char *>(exi::exi_malloc(AllocSize));
    if (NewMeta == nullptr)
      std::terminate();
    BlockList = new (NewMeta) BlockMeta{BlockList, 0};
  }

  void* allocateMassive(size_t NBytes) {
    NBytes += sizeof(BlockMeta);
    BlockMeta* NewMeta = reinterpret_cast<BlockMeta*>(exi::exi_malloc(NBytes));
    if (NewMeta == nullptr)
      std::terminate();
    BlockList->Next = new (NewMeta) BlockMeta{BlockList->Next, 0};
    return static_cast<void*>(NewMeta + 1);
  }

public:
  BumpPointerAllocator()
      : BlockList(new (InitialBuffer) BlockMeta{nullptr, 0}) {}

  void* allocate(size_t N) {
    N = (N + 15u) & ~15u;
    if (N + BlockList->Current >= UsableAllocSize) {
      if (N > UsableAllocSize)
        return allocateMassive(N);
      grow();
    }
    BlockList->Current += N;
    return static_cast<void*>(reinterpret_cast<char*>(BlockList + 1) +
                              BlockList->Current - N);
  }

  void reset() {
    while (BlockList) {
      BlockMeta* Tmp = BlockList;
      BlockList = BlockList->Next;
      if (reinterpret_cast<char*>(Tmp) != InitialBuffer)
        exi::exi_free(Tmp);
    }
    BlockList = new (InitialBuffer) BlockMeta{nullptr, 0};
  }

  ~BumpPointerAllocator() { reset(); }
};

class DefaultAllocator {
  BumpPointerAllocator Alloc;

public:
  void reset() { Alloc.reset(); }

  template<typename T, typename ...Args> T *makeNode(Args &&...args) {
    return new (Alloc.allocate(sizeof(T)))
        T(std::forward<Args>(args)...);
  }

  void *allocateNodeArray(size_t sz) {
    return Alloc.allocate(sizeof(Node *) * sz);
  }

  char *internString(std::string_view sv) {
    auto* Out = (char *)Alloc.allocate(sv.size() + 1);
    std::memcpy(Out, sv.data(), sv.size());
    Out[sv.size()] = '\0';
    return Out;
  }
};
}  // unnamed namespace

using Demangler = itanium_demangle::ManglingParser<DefaultAllocator>;

#define DUMP_SIMPLIFY 0

namespace {

#if DUMP_SIMPLIFY
using SimplifyVisitorBase = BasePrintVisitor;
#else
using SimplifyVisitorBase = BaseVisitor;
#endif

struct SimplifyVisitor : public SimplifyVisitorBase {
  Demangler& Parser;
  OutputBuffer OB;
#if DUMP_SIMPLIFY
  int Depth = 0;
#endif

  mutable PODSmallVector<const Node*, 32> NodeStack;
  bool IsExiFunction = false;
  bool AlreadyInNestedName = false;

  const Node* getNode(isize Pos = -1) const {
    if (Pos < 0) {
      const usize Off = std::abs(Pos) - 1;
      if EXI_LIKELY(Off < NodeStack.size())
        return NodeStack.end()[Pos];
    } else {
      if EXI_LIKELY(usize(Pos) < NodeStack.size())
        return NodeStack[Pos];
    }
    return nullptr;
  }
  Node* getNode(isize Pos = -1) {
    // We know the underlying type is mutable, so it's ok to cast.
    const Node* N = std::as_const(*this).getNode(Pos);
    return const_cast<Node*>(N);
  }

  /// Marks a node to be replaced.
  struct NodeReplacement {
    union {
      Node** Direct;
      const Node** ConstDirect;
    };
    const Node* ReplaceWith;
    /// If the type is actually const.
    bool IsReallyConst;
  };
  /// Holds the list of replacements.
  PODSmallVector<NodeReplacement, 8> Replacements;
  /// Marks a node to be overwritten.
  struct NodeOverwrite {
    Node* Overwrite;
    const Node* ReplaceWith;
    /// The size of the overwrite.
    u32 Size;
  };
  /// Holds the list of overwrites.
  PODSmallVector<NodeOverwrite, 4> Overwrites;

  // TODO: Use SmallPtrSet

  /// Holds the list of already handled items.
  exi::SmallPtrSet<const Node*, 8> AlreadyReplaced;
  exi::SmallPtrSet<const Node*, 4> AlreadyReplacedFunctions;

  using BaseVisitor::node_isa;
  using BaseVisitor::node_cast;
  using BaseVisitor::node_cast_if_present;
  using BaseVisitor::node_size;

  static const void* GetReplacementPoint(const NodeReplacement& R) {
    // Direct replacements
    if (R.IsReallyConst)
      return R.ConstDirect;
    else
      return R.Direct;
  }
  bool areAnyOtherReplacementsInsidePoint(const Node* Point, usize Size) {
    const char* const Begin = (const char*)Point;
    const char* const End = Begin + Size;
    for (const NodeReplacement& Replace : Replacements) {
      auto* P = (const char*)GetReplacementPoint(Replace);
      if (P >= Begin && P < End)
        return true;
      // Check our replacement isn't being overwritten either!
      auto* W = (const char*)Replace.ReplaceWith;
      if (W >= Begin && W < End)
        return true;
    }
    for (const NodeOverwrite& Overwrite : Overwrites) {
      auto* P = (const char*)Overwrite.Overwrite;
      if (P >= Begin && P < End)
        return true;
      // Check our replacement isn't being overwritten either!
      auto* W = (const char*)Overwrite.ReplaceWith;
      if (W >= Begin && W < End)
        return true;
    }
    return false;
  }

  template <typename PointT>
  void addReplacement(PointT** Point, const Node* ReplaceWith) {
    NodeReplacement Replacement {
      .ReplaceWith = ReplaceWith,
      .IsReallyConst = std::is_const_v<PointT>
    };
    if constexpr (!std::is_const_v<PointT>)
      Replacement.Direct = Point;
    else
      Replacement.ConstDirect = Point;
    Replacements.push_back(Replacement);
  }
  void addReplacement(const Node* Point, const Node* ReplaceWith) {
    const usize PointSize = node_size(Point);
    if (areAnyOtherReplacementsInsidePoint(Point, PointSize)) {
      // Other replacements would be inside this one!
      return;
    }
    // Check if we need to proxy our replacement.
    usize ReplaceWithSize = node_size(ReplaceWith);
    if (PointSize < ReplaceWithSize) {
      DEMANGLE_ASSERT(PointSize >= sizeof(NodeProxyNode),
                      "Replacement is not possible!");
      ReplaceWith = wrap(ReplaceWith);
      ReplaceWithSize = sizeof(NodeProxyNode);
    }
    Overwrites.push_back({
      .Overwrite = const_cast<Node*>(Point),
      .ReplaceWith = ReplaceWith,
      .Size = u32(ReplaceWithSize),
    });
  }

  void doReplacements() {
    for (const NodeReplacement& Replace : Replacements) {
      if (Replace.IsReallyConst)
        *Replace.ConstDirect = Replace.ReplaceWith;
      else {
        Node* N = const_cast<Node*>(Replace.ReplaceWith);
        *Replace.Direct = N;
      }
    }
    for (const NodeOverwrite& Overwrite : Overwrites) {
      const void* Src = Overwrite.ReplaceWith;
      void* Dst = Overwrite.Overwrite;
      std::memcpy(Dst, Src, Overwrite.Size);
    }
    // Clear it all.
    Replacements.clear();
    Overwrites.clear();
  }

  //////////////////////////////////////////////////////////////////////////////
  // Utility

  SimplifyVisitor(Demangler& Parser) : Parser(Parser) {}
  ~SimplifyVisitor() {
    if (char* Buf = OB.getBuffer())
      exi::exi_free(Buf);
  }

  template <class T, class...Args>
  ALWAYS_INLINE Node* make(Args&&...args) {
    return Parser.template make<T>(std::forward<Args>(args)...);
  }
  Node* makeName(std::string_view SV) {
    const char* Data = Parser.ASTAllocator.internString(SV);
    return make<NameType>(Data);
  }
  Node* wrap(const Node* N) {
    return make<NodeProxyNode>(const_cast<Node*>(N));
  }

  template <itanium_node NodeT, class...Args>
  static NodeT* reinit(const NodeT* N, Args&&...args) {
    return std::construct_at(const_cast<NodeT*>(N),
                             std::forward<Args>(args)...);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Dispatching

#if DUMP_SIMPLIFY
  void padding() {
    if (Depth <= 0) return;
    BasePrintVisitor::printPadding<' '>(Depth * 2);
  }
  void print(std::string_view SV) {
    this->padding();
    BasePrintVisitor::print(SV);
    BasePrintVisitor::printStr("\n");
  }
  void printNode(const Node* N) {
    if (!N) return;
    N->print(OB);
    this->print(OB);
    OB.setCurrentPosition(0);
  }
#else
  ALWAYS_INLINE constexpr void print(std::string_view SV) {}
  ALWAYS_INLINE constexpr void printNode(const Node*) {}
#endif

  class ScopedName {
#if DUMP_SIMPLIFY
    SimplifyVisitor& Visitor;
    const Node* N = nullptr;
    const char* Name = nullptr;
    bool Ptr : 1 = false;
    bool AlreadyReplaced : 1 = false;
  public:
    template <typename NodeT>
    ScopedName(SimplifyVisitor& thiz, const NodeT* N) : Visitor(thiz), N(N) {
      if (node_isa<NameWithTemplateArgs, NestedName, NameType>(N))
        this->Ptr = true;
      else if (node_isa<PointerType, ReferenceType>(N))
        this->Ptr = true;
      this->Name = node_name(N);
      Visitor.padding();
      if (this->Ptr)
        fprintf(stderr, "<%s:%p>", Name, N);
      else
        fprintf(stderr, "<%s>", Name);
      if (Visitor.AlreadyReplaced.contains(N)) {
        this->AlreadyReplaced = true;
        fputc('*', stderr);
      }
      fputc('\n', stderr);
      ++Visitor.Depth;
    }
    ~ScopedName() {
      --Visitor.Depth;
      Visitor.padding();
      if (this->Ptr)
        fprintf(stderr, "</%s:%p>", Name, N);
      else
        fprintf(stderr, "</%s>", Name);
      if (Visitor.AlreadyReplaced.contains(N))
        fputs(AlreadyReplaced ? "*" : "**", stderr);
      fputc('\n', stderr);
    }
#else
  public:
    ALWAYS_INLINE constexpr ScopedName(SimplifyVisitor&, const Node*) {}
#endif
  };

  void next(const Node* N) {
    if (N)
      N->visit(std::ref(*this));
  }
  void next(NodeArray A) {
    // Node array has data.
    for (const Node* N : A) {
      if EXI_LIKELY(N)
        N->visit(std::ref(*this));
    }
  }
  void next(...) {}

  //////////////////////////////////////////////////////////////////////////////
  // Specialization

  struct ArgVisitor {
    SimplifyVisitor& Visitor;
    template <typename...Ts> void operator()(Ts...Vs) {
      ((Visitor.next(Vs)), ...);
    }
  };

  template <typename NodeT> ALWAYS_INLINE void match(const NodeT* Node) {
    NodeStack.push_back(Node);
    Node->match(ArgVisitor{*this});
    NodeStack.pop_back();
  }

  template <typename NodeT> void operator()(const NodeT* Node) {
    ScopedName SN(*this, Node);
    exi::ScopedSave InNestedName(AlreadyInNestedName, false);
    match(Node);
  }
  void operator()(const ForwardTemplateReference* Node) {
    ScopedName SN(*this, Node);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Tree Modifications

  enum ComplexNameKind {
    CNK_none,
    CNK_exi,    // exi::*
    CNK_std__1, // std::__1::*
    CNK_xml,    // xml::*
  };

  /// Returns if a function is in the exi namespace.
  EXI_COLD EXI_NO_INLINE static bool
   isExiFunctionEncoding(const FunctionEncoding* FE) {
    DEMANGLE_ASSERT(FE, "Function cannot be null!");
    const NestedName* NN = node_cast<NestedName>(FE->getName());
    if (!NN) {
      if (auto* NWTA = node_cast<NameWithTemplateArgs>(FE->getName()))
        NN = node_cast<NestedName>(NWTA->Name);
      if (!NN) return false; // Global (or unknown), cannot be exi.
    }
    if (node_isa<NestedName>(NN->Qual)) {
      NN = getBaseNestedNamePair(const_cast<NestedName*>(NN)).first;
      if (!NN) return false;
    }
    // Check the actual name value.
    if (auto* Name = node_cast<NameType>(NN->Qual))
      return Name->getName() == "exi";
    return false;
  }

  static bool areBothNamesSimple(const NestedName* Node) {
    //return isa<NestedName>(Node->Qual) && isa<NameType>(Node->Name);
    return !node_isa<NestedName>(Node->Qual) /*&& node_isa<NameType>(Node->Name)*/;
  }
  static bool matchNameType(const Node* N, std::string_view ToMatch) {
    if (auto* Name = node_cast<NameType>(N))
      return Name->getName() == ToMatch;
    return false;
  }

  /// Loop through to find the base name pair.
  /// For example, with the name std::pmr::string, we would have:
  ///  [[[std, __1], pmr], string]
  ///
  /// which would return `{[std, __1], [[std, __1], pmr]}`.
  static std::pair<NestedName*, NestedName*>
   getBaseNestedNamePair(NestedName* const N) {
    NestedName *Last = N, *Curr = node_cast<NestedName>(N->Qual);
    if EXI_UNLIKELY(!Curr)
      return {nullptr, Last};
    // Loop through to find the name [std, __1].
    // For example, with the name std::pmr::string, we would have:
    //  [[[std, __1], pmr], string]
    while (!areBothNamesSimple(Curr)) {
      auto* Qual = node_cast<NestedName>(Curr->Qual);
      if EXI_UNLIKELY(!Qual)
        return {nullptr, Curr};
      Last = Curr;
      Curr = Qual;
    }
    // Found the base name!
    return {Curr, Last};
  }

  NameWithTemplateArgs* getLastNodeAsTArgs() {
    if (Node* Last = getNode(-1)) {
      if (auto* TArgs = node_cast<NameWithTemplateArgs>(Last))
        return TArgs;
    }
    return nullptr;
  }
  bool isLastNodeFunctionEncoding(const NestedName* Name) {
    if (auto* Last = getNode(-1)) {
      if (auto* FE = node_cast<FunctionEncoding>(Last))
        return FE->getName() == Name;
    }
    return false;
  }

  bool handle_ExiResult(NestedName* N) {
    auto* TName = getLastNodeAsTArgs();
    DEMANGLE_ASSERT(TName, "exi::Result without template arguments?");
    auto* TArgs = node_cast<TemplateArgs>(TName->TemplateArgs);
    if EXI_NEVER(!TArgs)
      return false;
    // Check the type of the E argument.
    NodeArray TParams = TArgs->getParams();
    if EXI_NEVER(TParams.size() != 2)
      return false;
    if (TParams[1]->getBaseName() != "ExiError")
      return false;
    // Create our new name and signature.
    Node* NewName = make<NameType>("ExiResult");
    Node* NewTArgs = make<TemplateArgs>(NodeArray(TParams.begin(), 1),
                                        TArgs->getRequires());
    AlreadyReplaced.insert(TName);
    addReplacement(&TName->Name, NewName);
    addReplacement(&TName->TemplateArgs, NewTArgs);
    return true;
  }

  bool handle_exi(NestedName* N, bool IsSimpleHint = false) {
    AlreadyReplaced.insert(N);
    if (N->getBaseName() == "AssertionKind") {
      addReplacement(N, N->Name);
      return true;
    } else if (N->getBaseName() == "Result") {
      if (handle_ExiResult(N))
        return true;
    }
    if (!IsSimpleHint && !areBothNamesSimple(N)) {
      // In this case we have something like [[exi, H], Foo].
      // This means a simple replacement can be done.
      auto [Curr, Last] = getBaseNestedNamePair(N);
      if EXI_UNLIKELY(!Curr) return false; // ???
      // Found the base name, so make sure it's correct!
      if (AlreadyReplaced.contains(Last)) return false;
      if (!matchNameType(Curr->Qual, "exi")) return false;
      // Set up the modification
      AlreadyReplaced.insert(Last);
      addReplacement(&Last->Qual, Curr->Name);
      return true;
    }
    // In this case we have something like [exi, Bar].
    // This complicates things, but some cases are simple.
    // To start with, let's try to replace a template.
    if (auto* TArgs = getLastNodeAsTArgs()) {
      if (TArgs->Name == N) {
        // Since this worked, we can do a simpler replacement.
        addReplacement(&TArgs->Name, N->Name);
        return true;
      }
    }
    // The last case didn't work, so we have to do a full replacement.
    addReplacement(N, N->Name);
    return true;
  }
  /// Convert `std::__1::*` to `std::*`.
  bool handle_std__1(NestedName* N) {
    auto [Curr, Last] = getBaseNestedNamePair(N);
    if EXI_UNLIKELY(!Curr) return false; // ???
    // Found the base name, so make sure it's correct!
    if (AlreadyReplaced.contains(Last)) return false;
    if (!matchNameType(Curr->Qual, "std")) return false;
    if (!matchNameType(Curr->Name, "__1")) return false;
    // Set up the modification
    AlreadyReplaced.insert(N);
    AlreadyReplaced.insert(Last);
    addReplacement(&Last->Qual, Curr->Qual);
    return true;
  }
  /// Strip the namespace from names matching `xml::XML*`.
  bool handle_xml(NestedName* N) {
    if (!areBothNamesSimple(N)) {
      AlreadyReplaced.insert(N);
      return false;
    }
    auto* TArgs = getLastNodeAsTArgs();
    if (!TArgs) {
      // Should be a template, so ignore this...
      AlreadyReplaced.insert(N);
      return false;
    }
    DEMANGLE_ASSERT(TArgs->Name == N, "Invalid state?");
    // Replace the inner name.
    // This is replicating the alias in the exi namespace, but we remove that,
    // so the namespace can be entirely replaced.
    if (AlreadyReplaced.contains(TArgs)) return false;
    AlreadyReplaced.insert(TArgs);
    addReplacement(TArgs, N->Name);
    return true;
  }

  static ComplexNameKind GetComplexNameKind(std::string_view Name) {
    if (starts_with(Name, "exi::"))
      return CNK_exi;
    else if (starts_with(Name, "std::__1::"))
      return CNK_std__1;
    else
      return CNK_none;
  }
  static bool IsXMLName(std::string_view BaseName) {
    if (!starts_with(BaseName, "XML"))
      return false;
    BaseName.remove_prefix(3);
    switch (BaseName.size()) {
    case 9:
      return BaseName == "Attribute";
    case 8:
      return BaseName == "Document";
    case 4:
      return BaseName == "Base" || BaseName == "Node";
    default:
      return false;
    }
  }

  bool handleComplexName(NestedName* Node) {
    if (this->AlreadyInNestedName) return false;
    else if (AlreadyReplaced.contains(Node)) return false;
    if (const auto* Qual = node_cast<NameType>(Node->Qual)) {
      if (isLastNodeFunctionEncoding(Node)) {
        AlreadyReplaced.insert(Node);
        return false;
      }
      std::string_view Name = Qual->getName();
      if (Name == "exi")
        return handle_exi(Node, true);
      else if (Name == "xml") {
        if (IsXMLName(Node->getBaseName()))
          return handle_xml(Node);
        return false;
      }
    }
    // Search with strings.
    Node->print(OB);
    const auto K = GetComplexNameKind(OB);
    OB.setCurrentPosition(0);
    if (K != CNK_std__1 && isLastNodeFunctionEncoding(Node)) {
      AlreadyReplaced.insert(Node);
      return false;
    }
    // Dispatch the handler.
    switch (K) {
    case CNK_exi:
      return handle_exi(Node);
    case CNK_std__1:
      return handle_std__1(Node);
    case CNK_xml:
    case CNK_none:
      return false;
    }
  }

  void operator()(const NestedName* Node) {
    // We know the underlying type is mutable, so it's ok to cast.
    auto* MutNode = const_cast<NestedName*>(Node);
    // Strip abi tags.
    if (auto* ABI = node_cast<AbiTagAttr>(Node->Name))
      MutNode->Name = ABI->Base;
    // Search for a special name...
    const bool InNestedName = handleComplexName(MutNode);
    const bool OldValue = AlreadyInNestedName;
    if (InNestedName)
      this->AlreadyInNestedName = true;
    ScopedName SN(*this, Node);
    this->printNode(Node);
    match(Node);
    // Restore the old value
    if (InNestedName)
      this->AlreadyInNestedName = OldValue;
  }

  void coalesceFunctionTArgs(const FunctionEncoding* FE) {
    auto* Name = node_cast<NameWithTemplateArgs>(
                   const_cast<Node*>(FE->getName()));
    if (!Name || AlreadyReplacedFunctions.contains(FE))
      return;
    AlreadyReplacedFunctions.insert(FE);
    auto* TArgs = node_cast<TemplateArgs>(Name->TemplateArgs);
    if (!TArgs || AlreadyReplaced.contains(TArgs)) return;

    // Check if any arguments are direct substitutions.
    SmallPtrSet<const Node*, 8> FnParams(exi::from_range, FE->getParams());
    SmallVec<Node*, 8> NewTParams;

    // Loop through the template params, see if any EXACTLY match fn params.
    // TODO: Handle parameter packs?
    NodeArray TParams = TArgs->getParams();
    for (Node* N : TParams) {
      if (!FnParams.contains(N))
        NewTParams.push_back(N);
    }
    if (NewTParams.size() == TParams.size()) {
      // No changes to the template.
      return;
    }

    if (NewTParams.empty()) {
      AlreadyReplaced.insert(Name);
      addReplacement(Name, Name->Name);
      return;
    }

    NodeArray NewTParamsForInit =
        Parser.makeNodeArray(NewTParams.begin(),
                             NewTParams.end());
    auto* NewTArgs = make<TemplateArgs>(NewTParamsForInit,
                                        TArgs->getRequires());
    AlreadyReplaced.insert(TArgs);
    addReplacement(&Name->TemplateArgs, NewTArgs);
  }

  void operator()(const FunctionEncoding* FE) {
    if EXI_UNLIKELY(NodeStack.empty())
      this->IsExiFunction = isExiFunctionEncoding(FE);
    coalesceFunctionTArgs(FE);
    ScopedName SN(*this, FE);
    exi::ScopedSave InNestedName(AlreadyInNestedName, false);
    match(FE);
  }

  void handleLambdaName(const NameType* Node) {
    // Check if this is a lambda.
    auto Name = Node->getName();
    if (!starts_with(Name, "$_")) return;
    if (AlreadyReplaced.contains(Node)) return;
    // Convert $_NNN into 'lambda#NNN'.
    Name.remove_prefix(2);
    OB << "'lambda<" << Name << ">'";
    const auto* NewNode = makeName(OB);
    OB.setCurrentPosition(0);
    // Add the new replacement.
    AlreadyReplaced.insert(Node);
    addReplacement(Node, NewNode);
  }

  void operator()(const NameType* Node) {
    handleLambdaName(Node);
    ScopedName SN(*this, Node);
    this->printNode(Node);
  }

  static const char* getExiFundamentalName(const FundamentalType* Node) {
    const auto K = Node->getFundamentalType();
    if (K == FundamentalTypeKind::Integer) {
      const bool Unsigned = Node->isUnsigned();
      switch (Node->getBits()) {
      case 8:   return Unsigned ? "u8"   : "i8";
      case 16:  return Unsigned ? "u16"  : "i16";
      case 32:  return Unsigned ? "u32"  : "i32";
      case 64:  return Unsigned ? "u64"  : "i64";
      case 128: return Unsigned ? "u128" : "i128";
      }
    }
    if (K == FundamentalTypeKind::Float) {
      if (Node->getBits() == 32) return "f32";
      if (Node->getBits() == 64) return "f64";
      if (Node->getBits() == 128) return "f128";
    }
    return nullptr;
  }
  const FundamentalType* handleFundamentalType(const FundamentalType* Node) {
    using K = FundamentalTypeKind;
    // Only transform in exi:: functions
    if (!IsExiFunction) return Node;
    // Only handle iN, uN, and fN
    const auto FTK = Node->getFundamentalType();
    if (FTK != K::Integer && FTK != K::Float)
      return Node;
    if (AlreadyReplaced.contains(Node))
      return Node;
    // Don't handle long double!
    if (Node->getBits() == 80)
      return Node;
    // Try and get the name of the type.
    const char* Name = getExiFundamentalName(Node);
    if EXI_UNLIKELY(Name == nullptr)
      return Node; // ?
    // Modify the node!
    AlreadyReplaced.insert(Node);
    return reinit(Node, Name, Node->getBits(),
                        FTK, Node->isUnsigned());
  }

  void operator()(const FundamentalType* Node) {
    Node = handleFundamentalType(Node);
    ScopedName SN(*this, Node);
    this->printNode(Node);
  }
};

struct XMLVisitor : public BasePrintVisitor {
  OutputBuffer OB;
  int Depth = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Utility

  ~XMLVisitor() {
    if (char* Buf = OB.getBuffer())
      exi::exi_free(Buf);
  }

  template <typename T> struct NamedArg {
    T Value;
    const char* Name = "";
  };

  template <typename NodeT>
  static constexpr bool isChild(const NodeT* Node) { return !!Node; }
  static bool isChild(NodeArray A) { return !A.empty(); }
  static constexpr bool isChild(...) { return false; }

  template <typename T>
  ALWAYS_INLINE static bool isChild(NamedArg<T> V) { return isChild(V.Value); }

  template <typename...Ts> static bool anyIsChild(Ts...Vs) {
    return (isChild(Vs) || ...);
  }

  template <typename NodeT>
  static constexpr bool isInline(const NodeT* Node) { return false; }
  static bool isInline(NodeArray) { return false; }
  static constexpr bool isInline(Node::Prec) { return false; }
  static constexpr bool isInline(...) { return true; }

  template <typename T>
  ALWAYS_INLINE static bool isInline(NamedArg<T> V) { return isInline(V.Value); }

  struct HasInlineVisitor {
    bool HasChildren = false;
    bool HasInline = false;
  
    template <typename...Ts> constexpr void operator()(Ts...Vs) {
      this->HasChildren = anyIsChild(Vs...);
      this->HasInline = (isInline(Vs) || ...);
    }
  };

  template <typename NodeT>
  static bool hasChildren(const NodeT* Node) {
    HasInlineVisitor V{};
    Node->match(std::ref(V));
    return V.HasChildren;
  }
  template <typename NodeT>
  static bool hasInline(const NodeT* Node) {
    HasInlineVisitor V{};
    Node->match(std::ref(V));
    return V.HasInline;
  }

  using BasePrintVisitor::printStr;
  using BasePrintVisitor::print;
  using BasePrintVisitor::printPadding;

  void print(const Node *N) = delete;
  void print(NodeArray A) = delete;

  void print(ReferenceKind RK) {
    print("ReferenceKind::*");
  }
  void print(FunctionRefQual RQ) {
    print("FunctionRefQual::*");
  }
  void print(Qualifiers Qs) {
    print("Qualifiers::*");
  }
  void print(FundamentalTypeKind FTK) {
    print("FundamentalTypeKind::*");
  }
  void print(SpecialSubKind SSK) {
    print("SpecialSubKind::*");
  }
  void print(TemplateParamKind TPK) {
    print("TemplateParamKind::*");
  }
  void print(Node::Prec P) = delete;

  template <typename T> void print(NamedArg<T> V) {
    if (V.Name && V.Name[0])
      fprintf(stderr, "%s=", V.Name);
    print(V.Value);
  }

  void padding() const {
    if EXI_LIKELY(Depth > 0)
      printPadding<' '>(Depth * 2u);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Node Printing

  template <typename T> void printInlineValue(unsigned& N, T V) {
    if (isInline(V)) {
      if constexpr (requires { this->print(V); }) {
        if (N > 0)
          fprintf(stderr, " arg%u=", N);
        else
          fprintf(stderr, " value=");
        print(V);
      }
      N += 1;
    }
  }
  template <typename T> void printInlineValue(unsigned& N, NamedArg<T> V) {
    if (!V.Name || !V.Name[0]) {
      printInlineValue(N, V.Value);
      return;
    } else if (isInline(V)) {
      if constexpr (requires { this->print(V.Value); }) {
        fprintf(stderr, " %s=", V.Name);
        print(V.Value);
      }
      N += 1;
    }
  }
  template <typename...Ts>
  void printInlineValues(unsigned& N, Ts...Vs) {
    ((printInlineValue(N, Vs)), ...);
  }
  void startInlineName(const char* Name) const {
    this->padding();
    fprintf(stderr, "<%s", Name);
  }
  void endInlineName(bool InlineOnly, unsigned N) const {
    static constexpr const char Data[] = " />\n";
    const unsigned StrOff = !InlineOnly ? 2 : (N > 0) ? 1 : 0;
    fprintf(stderr, "%s", Data + StrOff);
  }

  template <typename...Ts>
  unsigned startNameImpl(bool InlineOnly, const char* Name, Ts...Vs) {
    startInlineName(Name);
    unsigned N = 0;
    if constexpr (sizeof...(Ts) > 0)
      printInlineValues(N, Vs...);
    endInlineName(InlineOnly, N);
    return N;
  }
  template <class NodeT>
  bool startNameImpl(const NodeT* Node) {
    const bool HasChildren = hasChildren(Node);
    auto Functor = [this, HasChildren] (auto...Vs) {
      const char* Name = itanium_demangle::NodeKind<NodeT>::name();
      const bool InlineOnly = !HasChildren;
      (void) this->startNameImpl(InlineOnly, Name, Vs...);
    };
    Node->match(std::ref(Functor));
    if (HasChildren)
      ++Depth;
    return HasChildren;
  }

  template <typename...Ts>
  ALWAYS_INLINE bool startName(const char* Name, Ts...Vs) {
    (void) startNameImpl(/*InlineOnly=*/false, Name, Vs...);
    ++Depth;
    return true;
  }
  template <typename...Ts>
  ALWAYS_INLINE bool inlineName(const char* Name, Ts...Vs) {
    (void) startNameImpl(/*InlineOnly=*/true, Name, Vs...);
    return false;
  }
  /// @return `true` if the node has children, otherwise `false`.
  template <class NodeT>
  bool startName(const NodeT* Node) {
    const bool HasChildren = hasChildren(Node);
    auto Functor = [this, HasChildren] (auto...Vs) {
      const char* Name = NodeKind<NodeT>::name();
      const bool InlineOnly = !HasChildren;
      (void) this->startNameImpl(InlineOnly, Name, Vs...);
    };
    Node->match(std::ref(Functor));
    if (HasChildren)
      ++Depth;
    return HasChildren;
  }
  void endName(const char* Name) {
    --Depth;
    this->padding();
    fprintf(stderr, "</%s>\n", Name);
  }

  void comment(std::string_view SV) {
    this->padding();
    printStr("<!-- ");
    printStr(SV.data(), SV.size());
    printStr(" -->");
  }

  //////////////////////////////////////////////////////////////////////////////
  // Dispatching

  class ScopedName {
    XMLVisitor& Visitor;
    const char* Name = nullptr;
  public:
    template <typename NodeT>
    ScopedName(XMLVisitor& thiz, const NodeT* N) : Visitor(thiz) {
      if EXI_NEVER(N == nullptr)
        return;
      if (!Visitor.startName(N))
        return;
      this->Name = NodeKind<NodeT>::name();
    }
    ALWAYS_INLINE ScopedName(XMLVisitor& thiz, const char* Name)
        : Visitor(thiz), Name(Name) {
      Visitor.startName(Name); 
    }
    ~ScopedName() {
      if (Name)
        Visitor.endName(Name);
    }
  };

  void next(const Node* N) {
    if (N)
      N->visit(std::ref(*this));
  }
  void next(NodeArray A, const char* Name = nullptr) {
    if (!Name || !Name[0])
      Name = "NodeArray";
    // Check if this array is empty
    if (A.size() == 0) {
      this->inlineName(Name);
      return;
    }
    // Node array has data.
    ScopedName SN(*this, Name);
    for (const Node* N : A) {
      if EXI_LIKELY(N)
        N->visit(std::ref(*this));
    }
  }
  template <typename T>
  ALWAYS_INLINE void next(NamedArg<T> V) {
    next(V.Value);
  }
  ALWAYS_INLINE void next(NamedArg<NodeArray> A) {
    next(A.Value, A.Name);
  }
  void next(...) {}

  //////////////////////////////////////////////////////////////////////////////
  // Specialization

  struct ArgVisitor {
    XMLVisitor& Visitor;
    template <typename...Ts> void operator()(Ts...Vs) {
      ((Visitor.next(Vs)), ...);
    }
  };

  template <typename NodeT> ALWAYS_INLINE void match(const NodeT* Node) {
    Node->match(ArgVisitor{*this});
  }

  template <typename NodeT> void operator()(const NodeT* Node) {
    ScopedName SN(*this, Node);
    match(Node);
  }

  void operator()(const FunctionEncoding* Node) {
    ScopedName SN(*this, Node);
    ArgVisitor{*this}(
      NamedArg{Node->getReturnType(), "ReturnType"},
      NamedArg{Node->getName(), "FunctionName"},
      NamedArg{Node->getParams(), "FunctionParams"},
      NamedArg{Node->getAttrs(), "FunctionAttrs"},
      NamedArg{Node->getRequires(), "Requires"},
      NamedArg{Node->getCVQuals(), "CVQuals"},
      NamedArg{Node->getRefQual(), "RefQual"}
    );
  }

  void operator()(const ForwardTemplateReference* Node) {
    if (Node->Ref && !Node->Printing) {
      ScopedName SN(*this, "ForwardTemplateReference");
      Node->Printing = true;
      ArgVisitor{*this}(Node->Ref);
      Node->Printing = false;
    } else {
      inlineName("ForwardTemplateReference", Node->Index);
    }
  }
};
} // namespace `anonymous`

char *exi::itaniumDemangle(std::string_view MangledName, bool ParseParams) {
  if (MangledName.empty())
    return nullptr;

  Demangler Parser(MangledName.data(),
                   MangledName.data() + MangledName.length());
  Node *AST = Parser.parse(ParseParams);
  if (!AST)
    return nullptr;

  OutputBuffer OB;
  assert(Parser.ForwardTemplateRefs.empty());
  AST->print(OB);
  OB += '\0';
  return OB.getBuffer();
}

char *exi::itaniumDemangleSimple(std::string_view MangledName, bool ParseParams) {
  if (MangledName.empty())
    return nullptr;

  Demangler Parser(MangledName.data(),
                   MangledName.data() + MangledName.length());
  Node *AST = Parser.parse(ParseParams);
  if (!AST)
    return nullptr;
  
  SimplifyVisitor V(Parser);
  AST->visit(std::ref(V));
  V.doReplacements();

#if !DUMP_SIMPLIFY && 0
  XMLVisitor XMLV;
  AST->visit(std::ref(XMLV));
#endif

  OutputBuffer OB;
  assert(Parser.ForwardTemplateRefs.empty());
  AST->print(OB);
  OB += '\0';
  return OB.getBuffer();
}

ItaniumPartialDemangler::ItaniumPartialDemangler()
    : RootNode(nullptr), Context(new Demangler{nullptr, nullptr}) {}

ItaniumPartialDemangler::~ItaniumPartialDemangler() {
  delete static_cast<Demangler *>(Context);
}

ItaniumPartialDemangler::ItaniumPartialDemangler(
    ItaniumPartialDemangler &&Other)
    : RootNode(Other.RootNode), Context(Other.Context) {
  Other.Context = Other.RootNode = nullptr;
}

ItaniumPartialDemangler &ItaniumPartialDemangler::
operator=(ItaniumPartialDemangler &&Other) {
  std::swap(RootNode, Other.RootNode);
  std::swap(Context, Other.Context);
  return *this;
}

// Demangle MangledName into an AST, storing it into this->RootNode.
bool ItaniumPartialDemangler::partialDemangle(const char *MangledName) {
  Demangler *Parser = static_cast<Demangler *>(Context);
  size_t Len = std::strlen(MangledName);
  Parser->reset(MangledName, MangledName + Len);
  RootNode = Parser->parse();
  return RootNode == nullptr;
}
static char *printNode(const Node *RootNode, OutputBuffer &OB, size_t *N) {
  RootNode->print(OB);
  OB += '\0';
  if (N != nullptr)
    *N = OB.getCurrentPosition();
  return OB.getBuffer();
}

static char *printNode(const Node *RootNode, char *Buf, size_t *N) {
  OutputBuffer OB(Buf, N);
  return printNode(RootNode, OB, N);
}

char *ItaniumPartialDemangler::getFunctionBaseName(char *Buf, size_t *N) const {
  if (!isFunction())
    return nullptr;

  const Node *Name = static_cast<const FunctionEncoding *>(RootNode)->getName();

  while (true) {
    switch (Name->getKind()) {
    case Node::KAbiTagAttr:
      Name = static_cast<const AbiTagAttr *>(Name)->Base;
      continue;
    case Node::KModuleEntity:
      Name = static_cast<const ModuleEntity *>(Name)->Name;
      continue;
    case Node::KNestedName:
      Name = static_cast<const NestedName *>(Name)->Name;
      continue;
    case Node::KLocalName:
      Name = static_cast<const LocalName *>(Name)->Entity;
      continue;
    case Node::KNameWithTemplateArgs:
      Name = static_cast<const NameWithTemplateArgs *>(Name)->Name;
      continue;
    default:
      return printNode(Name, Buf, N);
    }
  }
}

char *ItaniumPartialDemangler::getFunctionDeclContextName(char *Buf,
                                                          size_t *N) const {
  if (!isFunction())
    return nullptr;
  const Node *Name = static_cast<const FunctionEncoding *>(RootNode)->getName();

  OutputBuffer OB(Buf, N);

 KeepGoingLocalFunction:
  while (true) {
    if (Name->getKind() == Node::KAbiTagAttr) {
      Name = static_cast<const AbiTagAttr *>(Name)->Base;
      continue;
    }
    if (Name->getKind() == Node::KNameWithTemplateArgs) {
      Name = static_cast<const NameWithTemplateArgs *>(Name)->Name;
      continue;
    }
    break;
  }

  if (Name->getKind() == Node::KModuleEntity)
    Name = static_cast<const ModuleEntity *>(Name)->Name;

  switch (Name->getKind()) {
  case Node::KNestedName:
    static_cast<const NestedName *>(Name)->Qual->print(OB);
    break;
  case Node::KLocalName: {
    auto *LN = static_cast<const LocalName *>(Name);
    LN->Encoding->print(OB);
    OB += "::";
    Name = LN->Entity;
    goto KeepGoingLocalFunction;
  }
  default:
    break;
  }
  OB += '\0';
  if (N != nullptr)
    *N = OB.getCurrentPosition();
  return OB.getBuffer();
}

char *ItaniumPartialDemangler::getFunctionName(char *Buf, size_t *N) const {
  if (!isFunction())
    return nullptr;
  auto *Name = static_cast<FunctionEncoding *>(RootNode)->getName();
  return printNode(Name, Buf, N);
}

char *ItaniumPartialDemangler::getFunctionParameters(char *Buf,
                                                     size_t *N) const {
  if (!isFunction())
    return nullptr;
  NodeArray Params = static_cast<FunctionEncoding *>(RootNode)->getParams();

  OutputBuffer OB(Buf, N);

  OB += '(';
  Params.printWithComma(OB);
  OB += ')';
  OB += '\0';
  if (N != nullptr)
    *N = OB.getCurrentPosition();
  return OB.getBuffer();
}

char *ItaniumPartialDemangler::getFunctionReturnType(
    char *Buf, size_t *N) const {
  if (!isFunction())
    return nullptr;

  OutputBuffer OB(Buf, N);

  if (const Node *Ret =
          static_cast<const FunctionEncoding *>(RootNode)->getReturnType())
    Ret->print(OB);

  OB += '\0';
  if (N != nullptr)
    *N = OB.getCurrentPosition();
  return OB.getBuffer();
}

char *ItaniumPartialDemangler::finishDemangle(char *Buf, size_t *N) const {
  assert(RootNode != nullptr && "must call partialDemangle()");
  return printNode(static_cast<Node *>(RootNode), Buf, N);
}

char *ItaniumPartialDemangler::finishDemangle(void *OB) const {
  assert(RootNode != nullptr && "must call partialDemangle()");
  assert(OB != nullptr && "valid OutputBuffer argument required");
  return printNode(static_cast<Node *>(RootNode),
                   *static_cast<OutputBuffer *>(OB),
                   /*N=*/nullptr);
}

bool ItaniumPartialDemangler::hasFunctionQualifiers() const {
  assert(RootNode != nullptr && "must call partialDemangle()");
  if (!isFunction())
    return false;
  auto *E = static_cast<const FunctionEncoding *>(RootNode);
  return E->getCVQuals() != QualNone || E->getRefQual() != FrefQualNone;
}

bool ItaniumPartialDemangler::isCtorOrDtor() const {
  const Node *N = static_cast<const Node *>(RootNode);
  while (N) {
    switch (N->getKind()) {
    default:
      return false;
    case Node::KCtorDtorName:
      return true;

    case Node::KAbiTagAttr:
      N = static_cast<const AbiTagAttr *>(N)->Base;
      break;
    case Node::KFunctionEncoding:
      N = static_cast<const FunctionEncoding *>(N)->getName();
      break;
    case Node::KLocalName:
      N = static_cast<const LocalName *>(N)->Entity;
      break;
    case Node::KNameWithTemplateArgs:
      N = static_cast<const NameWithTemplateArgs *>(N)->Name;
      break;
    case Node::KNestedName:
      N = static_cast<const NestedName *>(N)->Name;
      break;
    case Node::KModuleEntity:
      N = static_cast<const ModuleEntity *>(N)->Name;
      break;
    }
  }
  return false;
}

bool ItaniumPartialDemangler::isFunction() const {
  assert(RootNode != nullptr && "must call partialDemangle()");
  return static_cast<const Node *>(RootNode)->getKind() ==
         Node::KFunctionEncoding;
}

bool ItaniumPartialDemangler::isSpecialName() const {
  assert(RootNode != nullptr && "must call partialDemangle()");
  auto K = static_cast<const Node *>(RootNode)->getKind();
  return K == Node::KSpecialName || K == Node::KCtorVtableSpecialName;
}

bool ItaniumPartialDemangler::isData() const {
  return !isFunction() && !isSpecialName();
}
