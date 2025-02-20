#pragma once

#ifndef RAPIDXML_HPP_INCLUDED
#define RAPIDXML_HPP_INCLUDED

// Copyright (C) 2006, 2009 Marcin Kalicinski
// Version 1.13
// Revision $DateTime: 2009/05/13 01:46:17 $
//! \file rapidxml.hpp This file contains xml parser and DOM implementation

// If standard library is disabled, user must provide implementations of
// required functions and typedefs
#if !defined(RAPIDXML_NO_STDLIB)
# include <cassert> // For assert
# include <new>     // For placement new
#endif

#include <Common/Fundamental.hpp>
#include <Common/Option.hpp>
#include <Common/StrRef.hpp>
#include <Common/PointerIntPair.hpp>
#include <Support/Allocator.hpp>
#include <Support/Alignment.hpp>
#include <Support/ErrorHandle.hpp>
#include <string>

// On MSVC, disable "conditional expression is constant" warning (level 4).
// This warning is almost impossible to avoid with certain types of templated
// Code
#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable : 4127) // Conditional expression is constant
#endif

///////////////////////////////////////////////////////////////////////////
// RAPIDXML_PARSE_ERROR

#ifdef RAPIDXML_NO_EXCEPTIONS

# define RAPIDXML_PARSE_ERROR(what, where)                                     \
do {                                                                           \
  parse_error_handler(what, where);                                            \
  exi_unreachable("'parse_error_handler' returned!");                          \
} while(false)

namespace xml {
//! When exceptions are disabled by defining `RAPIDXML_NO_EXCEPTIONS`,
//! this function is called to notify user about the error.
//! It must be defined by the user.
//! <br><br>
//! This function cannot return. If it does, the results are undefined.
//! <br><br>
//! A very simple definition might look like that:
//! <pre>
//! void %xml::%parse_error_handler(const char *what, void *where) {
//!   outs() << "Parse error: " << what << "\n";
//!   std::abort();
//! }
//! </pre>
//! \param what Human readable description of the error.
//! \param where Pointer to character data where error was detected.
void parse_error_handler(const char* what, void* where);

//! Forces the use of exceptions. Useful for testing.
extern bool use_exceptions_anyway;

} // namespace xml

#else

# include <exception> // For std::exception

# define RAPIDXML_PARSE_ERROR(what, where) throw parse_error(what, where)

namespace xml {

//! Parse error exception.
//! This exception is thrown by the parser when an error occurs.
//! Use what() function to get human-readable error message.
//! Use where() function to get a pointer to position within source Text where
//! error was detected. <br><br> If throwing exceptions by the parser is
//! undesirable, it can be disabled by defining `RAPIDXML_NO_EXCEPTIONS` macro
//! before rapidxml.hpp is included. This will cause the parser to call
//! `xml::parse_error_handler()` function instead of throwing an exception.
//! This function must be defined by the user.
//! <br><br>
//! This class derives from `std::exception` class.
class parse_error : public std::exception {

public:
  //! Constructs parse error
  parse_error(const char* what, void* where) : m_what(what), m_where(where) {}

  //! Gets human readable description of error.
  //! \return Pointer to null terminated description of the error.
  virtual const char* what() const throw() { return m_what; }

  //! Gets pointer to character data where error happened.
  //! Ch should be the same as char type of XMLDocument that produced the
  //! error. \return Pointer to location within the parsed string where error
  //! occured.
  template <class Ch> Ch* where() const { return reinterpret_cast<Ch*>(m_where); }

private:
  const char* m_what;
  void* m_where;
};

//! Does nothing in this mode.
extern bool use_exceptions_anyway;

} // namespace xml

#endif

///////////////////////////////////////////////////////////////////////////
// Pool sizes

#ifndef RAPIDXML_STATIC_POOL_SIZE
// Size of static memory block of MemoryPool.
// Define RAPIDXML_STATIC_POOL_SIZE before including rapidxml.hpp if you want to
// override the default value. No dynamic memory allocations are performed by
// MemoryPool until static memory is exhausted.
# define RAPIDXML_STATIC_POOL_SIZE (64 * 1024)
#endif

#ifndef RAPIDXML_DYNAMIC_POOL_SIZE
// Size of dynamic memory block of MemoryPool.
// Define RAPIDXML_DYNAMIC_POOL_SIZE before including rapidxml.hpp if you want
// to override the default value. After the static block is exhausted, dynamic
// blocks with approximately this size are allocated by MemoryPool.
# define RAPIDXML_DYNAMIC_POOL_SIZE (64 * 1024)
#endif

#ifndef RAPIDXML_ALIGNMENT
// Memory allocation alignment.
// Define RAPIDXML_ALIGNMENT before including rapidxml.hpp if you want to
// override the default value, which is the size of pointer. All memory
// allocations for nodes, attributes and strings will be aligned to this value.
// This must be a power of 2 and at least 1, otherwise MemoryPool will not
// work.
# define RAPIDXML_ALIGNMENT sizeof(void*)
#endif

#define RAPIDXML_ALIASES(TYPE)                                                \
using StrRefT  = ::xml::internal::string_type_t<TYPE>;                        \
using NodeType = ::xml::XMLNode<TYPE>;                                        \
using AttrType = ::xml::XMLAttribute<TYPE>;                                   \
using DocType  = ::xml::XMLDocument<TYPE>;

namespace xml {
// Forward declarations
template <class Ch> class XMLNode;
template <class Ch> class XMLAttribute;
template <class Ch> class XMLDocument;

//! Enumeration listing all node types produced by the parser.
//! Use XMLNode::type() function to query node type.
enum NodeKind {
  node_document,    //!< A document node. Name and value are empty.
  node_element,     //!< An element node. Name contains element name. Value contains Text of first data node.
  node_data,        //!< A data node. Name is empty. Value contains data Text.
  node_cdata,       //!< A CDATA node. Name is empty. Value contains data Text.
  node_comment,     //!< A comment node. Name is empty. Value contains comment Text.
  node_declaration, //!< A declaration node. Name and value are empty. Declaration parameters (version, encoding and standalone) are in node attributes.
  node_doctype,     //!< A DOCTYPE node. Name is empty. Value contains DOCTYPE Text.
  node_pi           //!< A PI node. Name contains target. Value contains instructions.
};

using XMLBumpAllocator = exi::BumpPtrAllocator;

inline constexpr usize kPoolSize = RAPIDXML_DYNAMIC_POOL_SIZE;
inline constexpr usize kAlignVal = RAPIDXML_ALIGNMENT;
inline constexpr exi::Align kAlign(kAlignVal);

///////////////////////////////////////////////////////////////////////
// Parsing flags

//! Parse flag instructing the parser to not create data nodes.
//! Text of first data node will still be placed in value of parent element,
//! unless xml::parse_no_element_values flag is also specified. Can be
//! combined with other flags by use of | operator. <br><br> See
//! XMLDocument::parse() function.
constexpr int parse_no_data_nodes = 0x1;

//! Parse flag instructing the parser to not use Text of first data node as a
//! value of parent element. Can be combined with other flags by use of |
//! operator. Note that child data nodes of element node take precendence over
//! its value when printing. That is, if element has one or more child data
//! nodes <em>and</em> a value, the value will be ignored. Use
//! xml::parse_no_data_nodes flag to prevent creation of data nodes if you
//! want to manipulate data using values of elements. <br><br> See
//! XMLDocument::parse() function.
constexpr int parse_no_element_values = 0x2;

//! Parse flag instructing the parser to not place zero terminators after
//! strings in the source Text. By default zero terminators are placed,
//! modifying source Text. Can be combined with other flags by use of |
//! operator. <br><br> See XMLDocument::parse() function.
constexpr int parse_no_string_terminators = 0x4;

//! Parse flag instructing the parser to not translate entities in the source
//! Text. By default entities are translated, modifying source Text. Can be
//! combined with other flags by use of | operator. <br><br> See
//! XMLDocument::parse() function.
constexpr int parse_no_entity_translation = 0x8;

//! Parse flag instructing the parser to disable UTF-8 handling and assume plain
//! 8 bit characters. By default, UTF-8 handling is enabled. Can be combined
//! with other flags by use of | operator. <br><br> See XMLDocument::parse()
//! function.
constexpr int parse_no_utf8 = 0x10;

//! Parse flag instructing the parser to create XML declaration node.
//! By default, declaration node is not created.
//! Can be combined with other flags by use of | operator.
//! <br><br>
//! See XMLDocument::parse() function.
constexpr int parse_declaration_node = 0x20;

//! Parse flag instructing the parser to create comments nodes.
//! By default, comment nodes are not created.
//! Can be combined with other flags by use of | operator.
//! <br><br>
//! See XMLDocument::parse() function.
constexpr int parse_comment_nodes = 0x40;

//! Parse flag instructing the parser to create DOCTYPE node.
//! By default, doctype node is not created.
//! Although W3C specification allows at most one DOCTYPE node, RapidXml will
//! silently accept documents with more than one. Can be combined with other
//! flags by use of | operator. <br><br> See XMLDocument::parse() function.
constexpr int parse_doctype_node = 0x80;

//! Parse flag instructing the parser to create PI nodes.
//! By default, PI nodes are not created.
//! Can be combined with other flags by use of | operator.
//! <br><br>
//! See XMLDocument::parse() function.
constexpr int parse_pi_nodes = 0x100;

//! Parse flag instructing the parser to validate closing tag names.
//! If not set, name inside closing tag is irrelevant to the parser.
//! By default, closing tags are not validated.
//! Can be combined with other flags by use of | operator.
//! <br><br>
//! See XMLDocument::parse() function.
constexpr int parse_validate_closing_tags = 0x200;

//! Parse flag instructing the parser to trim all leading and trailing
//! whitespace of data nodes. By default, whitespace is not trimmed. This flag
//! does not cause the parser to modify source Text. Can be combined with other
//! flags by use of | operator. <br><br> See XMLDocument::parse() function.
constexpr int parse_trim_whitespace = 0x400;

//! Parse flag instructing the parser to condense all whitespace runs of data
//! nodes to a single space character. Trimming of leading and trailing
//! whitespace of data is controlled by xml::parse_trim_whitespace flag. By
//! default, whitespace is not normalized. If this flag is specified, source
//! Text will be modified. Can be combined with other flags by use of |
//! operator. <br><br> See XMLDocument::parse() function.
constexpr int parse_normalize_whitespace = 0x800;

//! Parse flag instructing the parser to convert all newline types to a single
//! character. By default, newlines are not normalized. If this flag is
//! specified, source Text will be modified. Can be combined with other flags by
//! use of | operator. <br><br> See XMLDocument::parse() function.
constexpr int parse_normalize_newlines = 0x1000;

// Compound flags

//! Parse flags which represent default behaviour of the parser.
//! This is always equal to 0, so that all other flags can be simply ored
//! together. Normally there is no need to inconveniently disable flags by
//! anding with their negated (~) values. This also means that meaning of each
//! flag is a <i>negation</i> of the default setting. For example, if flag name
//! is xml::parse_no_utf8, it means that utf-8 is <i>enabled</i> by
//! default, and using the flag will disable it. <br><br> See
//! XMLDocument::parse() function.
constexpr int parse_default = 0;

//! A combination of parse flags that forbids any modifications of the source
//! Text. This also results in faster parsing. However, note that the following
//! will occur: <ul> <li>names and values of nodes will not be zero terminated,
//! you have to use XMLBase::name_size() and XMLBase::value_size() functions
//! to determine where name and value ends</li> <li>entities will not be
//! translated</li> <li>whitespace will not be normalized</li>
//! </ul>
//! See XMLDocument::parse() function.
constexpr int parse_non_destructive = parse_no_string_terminators | parse_no_entity_translation;

//! A combination of parse flags resulting in fastest possible parsing, without
//! sacrificing important data. <br><br> See XMLDocument::parse() function.
constexpr int parse_fastest = parse_non_destructive | parse_no_data_nodes;

//! A combination of parse flags resulting in most nodes being extracted,
//! without validation. This usually results in slower parsing. <br><br> See
//! XMLDocument::parse() function.
constexpr int parse_all = parse_comment_nodes | parse_doctype_node | parse_pi_nodes;

//! A combination of parse flags resulting in largest amount of data being
//! extracted. This usually results in slowest parsing. <br><br> See
//! XMLDocument::parse() function.
constexpr int parse_full = parse_declaration_node | parse_all | parse_validate_closing_tags;

///////////////////////////////////////////////////////////////////////
// Internals

//! \cond internal
namespace internal {
  // Struct that contains lookup tables for the parser
  // It must be a template to allow correct linking (because it has static data
  // members, which are defined in a header file).
  template <int Dummy> struct LookupTables {
    static const u8 whitespace[256];            // Whitespace table
    static const u8 node_name[256];             // Node name table
    static const u8 Text[256];                  // Text table
    static const u8 text_pure_no_ws[256];       // Text table
    static const u8 text_pure_with_ws[256];     // Text table
    static const u8 attribute_name[256];        // Attribute name table
    static const u8 attribute_data_1[256];      // Attribute data table with single quote
    static const u8 attribute_data_1_pure[256]; // Attribute data table with single quote
    static const u8 attribute_data_2[256];      // Attribute data table with double quotes
    static const u8 attribute_data_2_pure[256]; // Attribute data table with double quotes
    static const u8 digits[256];                // Digits
    static const u8 upcase[256];                // To uppercase conversion table for ASCII characters
  };

  template <class Ch> struct string_type {
    using type = std::basic_string_view<Ch>;
  };

  template <> struct string_type<char> {
    using type = exi::StrRef;
  };

#if defined(__cpp_char8_t)
  template <> struct string_type<char8_t> {
    using type = exi::StrRef;
  };
#endif

  template <class Ch>
  using string_type_t = typename string_type<char>::type;

  // Find length of the string
  template <class Ch> inline usize measure(const Ch* p) {
    return std::char_traits<Ch>::length(p);
  }

  // Compare strings for equality
  template <class Ch>
  inline bool compare(const Ch* p1, usize size1, const Ch* p2, usize size2,
                      bool CaseInsensitive) {
    if (size1 != size2)
      return false;
    if (CaseInsensitive) {
      for (const Ch* end = p1 + size1; p1 < end; ++p1, ++p2)
        if (*p1 != *p2)
          return false;
    } else {
      for (const Ch* end = p1 + size1; p1 < end; ++p1, ++p2)
        if (LookupTables<0>::upcase[u8(*p1)] !=
            LookupTables<0>::upcase[u8(*p2)])
          return false;
    }
    return true;
  }

  template <class Ch>
  inline bool compare(std::basic_string_view<Ch> Str1,
                      std::basic_string_view<Ch> Str2, bool CaseInsensitive) {
    return internal::compare(
      Str1.data(), Str1.size(),
      Str2.data(), Str2.size(), CaseInsensitive);
  }

  inline bool compare(exi::StrRef Str1, exi::StrRef Str2, bool CaseInsensitive) {
    if (not CaseInsensitive)
      return Str1.compare(Str2) == 0;
    return internal::compare(
      Str1.data(), Str1.size(),
      Str2.data(), Str2.size(), true);
  }
} // namespace internal
//! \endcond

///////////////////////////////////////////////////////////////////////
// Memory pool

//! This class is used by the parser to create new nodes and attributes, without
//! overheads of dynamic memory allocation. In most cases, you will not need to
//! use this class directly. However, if you need to create nodes manually or
//! modify names/values of nodes, you are encouraged to use MemoryPool of
//! relevant XMLDocument to allocate the memory. Not only is this faster than
//! allocating them by using `new` operator, but also their lifetime
//! will be tied to the lifetime of document, possibly simplyfing memory
//! management. <br><br> Call allocate_node() or allocate_attribute() functions
//! to obtain new nodes or attributes from the pool. You can also call
//! allocString() function to allocate strings. Such strings can then be
//! used as names or values of nodes without worrying about their lifetime. Note
//! that there is no `free()` function -- all allocations are freed
//! at once when clear() function is called, or when the pool is destroyed.
//! <br><br>
//! It is also possible to create a standalone MemoryPool, and use it
//! to allocate nodes, whose lifetime will not be tied to any document.
//! <br><br>
//! Pool maintains `RAPIDXML_STATIC_POOL_SIZE` bytes of statically
//! allocated memory. Until static memory is exhausted, no dynamic memory
//! allocations are done. When static memory is exhausted, pool allocates
//! additional blocks of memory of size `RAPIDXML_DYNAMIC_POOL_SIZE`
//! each, by using global `new[]` and `delete[]`
//! operators. This behaviour can be changed by setting custom allocation
//! routines. Use set_allocator() function to set them. <br><br> Allocations for
//! nodes, attributes and strings are aligned at `RAPIDXML_ALIGNMENT`
//! bytes. This value defaults to the size of pointer on target architecture.
//! <br><br>
//! To obtain absolutely top performance from the parser,
//! it is important that all nodes are allocated from a single, contiguous block
//! of memory. Otherwise, cache misses when jumping between two (or more)
//! disjoint blocks of memory can slow down parsing quite considerably. If
//! required, you can tweak `RAPIDXML_STATIC_POOL_SIZE`,
//! `RAPIDXML_DYNAMIC_POOL_SIZE` and `RAPIDXML_ALIGNMENT`
//! to obtain best wasted memory to performance compromise.
//! To do it, define their values before rapidxml.hpp file is included.
//! \param Ch Character type of created nodes.
template <class Ch = char> class MemoryPool {
  RAPIDXML_ALIASES(Ch)
  using TraitsT = std::char_traits<Ch>;
  // TODO: Make this less sucky
  exi::PointerIntPair<XMLBumpAllocator*, 1, bool> AllocBase;
public:
  //! Constructs empty pool.
  MemoryPool() : AllocBase(new XMLBumpAllocator, true) {}
  //! Constructs pool from input allocator.
  explicit MemoryPool(XMLBumpAllocator& A) : AllocBase(&A, false) { }

  //! Destroys pool and frees all the memory.
  //! This causes memory occupied by nodes allocated by the pool to be freed.
  //! Nodes allocated from the pool are no longer valid.
  ~MemoryPool() {
    this->clear();
    if (AllocBase.getInt())
      delete AllocBase.getPointer();
  }

  //! Allocates a new attribute from the pool, and optionally assigns name and
  //! value to it. If the allocation request cannot be accomodated, this
  //! function will throw `std::bad_alloc`. If exceptions are
  //! disabled by defining `RAPIDXML_NO_EXCEPTIONS`, this function will call
  //! `xml::parse_error_handler()` function.
  //! \param type Type of node to create.
  //! \param name Name to assign to the attribute, or 0 to assign no name.
  //! \param value Value to assign to the attribute, or 0 to assign no value.
  //! \param name_size Size of name to assign, or 0 to automatically calculate size from name string.
  //! \param value_size Size of value to assign, or 0 to automatically calculate size from value string.
  //! \return Pointer to allocated attribute. This pointer will never be NULL.
  EXI_RETURNS_NONNULL NodeType*
      allocate_node(NodeKind Kind, const Ch* Name = 0, const Ch* Value = 0,
                    usize NameLen = 0, usize ValueLen = 0) {
    auto* Node = new (Alloc()) NodeType(Kind);
    if (Name) {
      if (NameLen > 0)
        Node->name(Name, NameLen);
      else
        Node->name(Name);
    }
    if (Value) {
      if (ValueLen > 0)
        Node->value(Value, ValueLen);
      else
        Node->value(Value);
    }
    return Node;
  }

  EXI_RETURNS_NONNULL ALWAYS_INLINE NodeType*
      allocate_node(NodeKind Kind, StrRefT Name, StrRefT Value) {
    return allocate_attribute(Kind, Name.data(), Value.data(),
                                    Name.size(), Value.size());
  }

  //! Allocates a new attribute from the pool, and optionally assigns name and
  //! value to it. If the allocation request cannot be accomodated, this
  //! function will throw `std::bad_alloc`. If exceptions are
  //! disabled by defining `RAPIDXML_NO_EXCEPTIONS`, this function will call
  //! `xml::parse_error_handler()` function.
  //! \param name Name to assign to the attribute, or 0 to assign no name.
  //! \param value Value to assign to the attribute, or 0 to assign no value.
  //! \param name_size Size of name to assign, or 0 to automatically calculate size from name string.
  //! \param value_size Size of value to assign, or 0 to automatically calculate size from value string.
  //! \return Pointer to allocated attribute. This pointer will never be NULL.
  EXI_RETURNS_NONNULL AttrType*
                     allocate_attribute(const Ch* Name = 0, const Ch* Value = 0,
                                        usize NameLen = 0,  usize ValueLen = 0) {
    auto* Attr = new (Alloc()) AttrType();
    if (Name) {
      if (NameLen > 0)
        Attr->name(Name, NameLen);
      else
        Attr->name(Name);
    }
    if (Value) {
      if (ValueLen > 0)
        Attr->value(Value, ValueLen);
      else
        Attr->value(Value);
    }
    return Attr;
  }

  EXI_RETURNS_NONNULL ALWAYS_INLINE AttrType*
      allocate_attribute(StrRefT Name, StrRefT Value) {
    return allocate_attribute(Name.data(), Value.data(),
                              Name.size(), Value.size());
  }

  //! Allocates a char array of given size from the pool, and optionally copies
  //! a given string to it. If the allocation request cannot be accomodated,
  //! this function will throw `std::bad_alloc`. If exceptions are
  //! disabled by defining `RAPIDXML_NO_EXCEPTIONS`, this function will call
  //! `xml::parse_error_handler()` function.
  //! \param Src String to initialize the allocated memory with, or 0 to not initialize it.
  //! \param Size Number of characters to allocate, or zero to calculate it
  //! automatically from source string length; if size is 0, source string must
  //! be specified and null terminated.
  //! \return Pointer to allocated char array. This pointer will never be NULL.
  EXI_RETURNS_NONNULL Ch* allocString(const Ch* Src = nullptr, usize Size = 0) {
    assert(Src || Size); // Either source or size (or both) must be specified
    if (Size == 0)
      Size = internal::measure(Src) + 1;
    Ch* Out = AllocString(Alloc(), Size);
    if (Src)
      TraitsT::copy(Out, Src, Size);
    return Out;
  }

  //! Clones an XMLNode and its hierarchy of child nodes and attributes.
  //! Nodes and attributes are allocated from this memory pool.
  //! Names and values are not cloned, they are shared between the clone and the
  //! source. Result node can be optionally specified as a second parameter, in
  //! which case its contents will be replaced with cloned source node. This is
  //! useful when you want to clone entire document.
  //! \param source Node to clone.
  //! \param result Node to put results in, or 0 to automatically allocate result node
  //! \return Pointer to cloned node. This pointer will never be NULL.
  EXI_RETURNS_NONNULL XMLNode<Ch>* clone_node(const XMLNode<Ch>* Src, 
                                               XMLNode<Ch>* Out = nullptr) {  
    // Prepare result node
    if (Out) {
      Out->remove_all_attributes();
      Out->remove_all_nodes();
      Out->type(Src->type());
    } else {
      Out = allocate_node(Src->type());
    }

    // Clone name and value
    Out->name(Src->name(), Src->name_size());
    Out->value(Src->value(), Src->value_size());

    // Clone child nodes and attributes
    for (XMLNode<Ch>* Child = Src->first_node(); Child; Child = Child->next_sibling())
      Out->append_node(clone_node(Child));
    for (XMLAttribute<Ch>* Attr = Src->first_attribute(); Attr; Attr = Attr->next_attribute())
      Out->append_attribute(allocate_attribute(Attr->name(), Attr->value(),
                                               Attr->name_size(), Attr->value_size()));

    return Out;
  }

  //! Clears the pool.
  //! This causes memory occupied by nodes allocated by the pool to be freed.
  //! Any nodes or strings allocated from the pool will no longer be valid.
  void clear() {
    if (AllocBase.getInt())
      AllocBase->Reset();
  }

private:
  static char* align(char* Ptr) {
    const uptr Raw = exi::alignAddr(Ptr, kAlign);
    return reinterpret_cast<char*>(Raw);
  }

  XMLBumpAllocator& Alloc() {
    auto* const AllocPtr = AllocBase.getPointer();
    exi_invariant(AllocPtr != nullptr);
    return *AllocPtr;
  }

  char* allocRaw(usize Size) {
    void* Mem = AllocBase->Allocate(Size, 1);
    return static_cast<char*>(Mem);
  }

  void* allocAligned(usize Size) {
    return AllocBase->Allocate(Size, kAlign);
  }

  static Ch* AllocString(XMLBumpAllocator& Alloc, usize Size) {
    const usize TrueSize = Size * sizeof(Ch);
    void* Mem = Alloc.Allocate(TrueSize, kAlign);
    return static_cast<Ch*>(Mem);
  }
};

///////////////////////////////////////////////////////////////////////////
// XML base

//! Base class for XMLNode and XMLAttribute implementing common functions:
//! name(), name_size(), value(), value_size() and parent().
//! \param Ch Character type to use
template <class Ch = char>
class alignas(kAlignVal) XMLBase {
  RAPIDXML_ALIASES(Ch)
public:

  ///////////////////////////////////////////////////////////////////////////
  // Construction & destruction

  // Construct a base with empty name, value and parent
  XMLBase() : m_name(0), m_value(0), m_parent(0) {}

  ///////////////////////////////////////////////////////////////////////////
  // Node data access

  //! Gets name of the node.
  //! Interpretation of name depends on type of node.
  //! Note that name will not be zero-terminated if
  //! xml::parse_no_string_terminators option was selected during parse.
  //! <br><br>
  //! Use name_size() function to determine length of the name.
  //! \return Name of node, or empty string if node has no name.
  StrRefT name() const { return StrRefT(name_data(), name_size()); }

  //! Gets name of the node.
  //! Interpretation of name depends on type of node.
  //! Note that name will not be zero-terminated if
  //! xml::parse_no_string_terminators option was selected during parse.
  //! <br><br>
  //! Use name_size() function to determine length of the name.
  //! \return Name of node, or empty string if node has no name.
  Ch* name_data() const { return m_name ? m_name : nullstr(); }

  //! Gets size of node name, not including terminator character.
  //! This function works correctly irrespective of whether name is or is not
  //! zero terminated. \return Size of node name, in characters.
  usize name_size() const { return m_name ? m_name_size : 0; }

  //! Gets value of node.
  //! Interpretation of value depends on type of node.
  //! Note that value will not be zero-terminated if
  //! xml::parse_no_string_terminators option was selected during parse.
  //! <br><br>
  //! Use value_size() function to determine length of the value.
  //! \return Value of node, or empty string if node has no value.
  StrRefT value() const { return StrRefT(value_data(), value_size()); }

  //! Gets value of node.
  //! Interpretation of value depends on type of node.
  //! Note that value will not be zero-terminated if
  //! xml::parse_no_string_terminators option was selected during parse.
  //! <br><br>
  //! Use value_size() function to determine length of the value.
  //! \return Value of node, or empty string if node has no value.
  Ch* value_data() const { return m_value ? m_value : nullstr(); }

  //! Gets size of node value, not including terminator character.
  //! This function works correctly irrespective of whether value is or is not
  //! zero terminated. \return Size of node value, in characters.
  usize value_size() const { return m_value ? m_value_size : 0; }

  ///////////////////////////////////////////////////////////////////////////
  // Node modification

  //! Sets name of node to a non zero-terminated string.
  //! See \ref ownership_of_strings.
  //! <br><br>
  //! Note that node does not own its name or value, it only stores a pointer to
  //! it. It will not delete or otherwise free the pointer on destruction. It is
  //! reponsibility of the user to properly manage lifetime of the string. The
  //! easiest way to achieve it is to use MemoryPool of the document to
  //! allocate the string - on destruction of the document the string will be
  //! automatically freed. <br><br> Size of name must be specified separately,
  //! because name does not have to be zero terminated. Use name(const Ch *)
  //! function to have the length automatically calculated (string must be zero
  //! terminated).
  //! \param Name Name of node to set. Does not have to be zero terminated. 
  //! \param Size Size of name, in characters. This does not include zero
  //! terminator, if one is present.
  void name(const Ch* Name, usize Size) {
    m_name = const_cast<Ch*>(Name);
    m_name_size = Size;
  }

  //! Sets name of node to a zero-terminated string.
  //! See also \ref ownership_of_strings and XMLNode::name(const Ch*, usize).
  //! \param Name Name of node to set. Must be zero terminated.
  void name(StrRefT Str) { this->name(Str.data(), Str.size()); }

  //! Sets value of node to a non zero-terminated string.
  //! See \ref ownership_of_strings.
  //! <br><br>
  //! Note that node does not own its name or value, it only stores a pointer to
  //! it. It will not delete or otherwise free the pointer on destruction. It is
  //! reponsibility of the user to properly manage lifetime of the string. The
  //! easiest way to achieve it is to use MemoryPool of the document to
  //! allocate the string - on destruction of the document the string will be
  //! automatically freed. <br><br> Size of value must be specified separately,
  //! because it does not have to be zero terminated. Use value(const Ch *)
  //! function to have the length automatically calculated (string must be zero
  //! terminated). <br><br> If an element has a child node of type node_data, it
  //! will take precedence over element value when printing. If you want to
  //! manipulate data of elements using values, use parser flag
  //! xml::parse_no_data_nodes to prevent creation of data nodes by the
  //! parser. \param value value of node to set. Does not have to be zero
  //! terminated. \param size Size of value, in characters. This does not
  //! include zero terminator, if one is present.
  void value(const Ch* value, usize size) {
    m_value = const_cast<Ch*>(value);
    m_value_size = size;
  }

  //! Sets value of node to a zero-terminated string.
  //! See also \ref ownership_of_strings and XMLNode::value(const Ch *, usize).
  //! \param value Vame of node to set. Must be zero terminated.
  void value(StrRefT Str) { this->name(Str.data(), Str.size()); }

  ///////////////////////////////////////////////////////////////////////////
  // Related nodes access

  //! Gets node parent.
  //! \return Pointer to parent node, or 0 if there is no parent.
  NodeType* parent() const { return m_parent; }

protected:
  // Return empty string
  static Ch* nullstr() {
    static Ch zero = Ch('\0');
    return &zero;
  }

  Ch* m_name;         // Name of node, or 0 if no name
  Ch* m_value;        // Value of node, or 0 if no value
  usize m_name_size;  // Length of node name, or undefined of no name
  usize m_value_size; // Length of node value, or undefined if no value
  NodeType* m_parent; // Pointer to parent node, or 0 if none
};

//! Class representing attribute node of XML document.
//! Each attribute has name and value strings, which are available through
//! name() and value() functions (inherited from XMLBase). Note that after
//! parse, both name and value of attribute will point to interior of source
//! Text used for parsing. Thus, this Text must persist in memory for the
//! lifetime of attribute. \param Ch Character type to use.
template <class Ch = char>
class alignas(kAlignVal) XMLAttribute : public XMLBase<Ch> {
  friend class XMLNode<Ch>;
  RAPIDXML_ALIASES(Ch)
public:
  ///////////////////////////////////////////////////////////////////////////
  // Construction & destruction

  //! Constructs an empty attribute with the specified type.
  //! Consider using MemoryPool of appropriate XMLDocument if allocating
  //! attributes manually.
  XMLAttribute() {}

  ///////////////////////////////////////////////////////////////////////////
  // Related nodes access

  //! Gets document of which attribute is a child.
  //! \return Pointer to document that contains this attribute, or 0 if there is
  //! no parent document.
  DocType* document() const {
    if (NodeType* node = this->parent()) {
      while (node->parent())
        node = node->parent();
      if EXI_LIKELY(node->type() == node_document)
        return static_cast<DocType*>(node);
    }
    return nullptr;
  }

  //! Gets previous attribute, optionally matching attribute name.
  //! \param name Name of attribute to find, or 0 to return previous attribute
  //! regardless of its name; this string doesn't have to be zero-terminated if
  //! name_size is non-zero
  //! \param name_size Size of name, in characters, or 0 to have size calculated automatically from string
  //! \param CaseInsensitive Should name comparison be case-sensitive; non case-sensitive comparison
  //! works properly only for ASCII characters
  //! \return Pointer to found attribute, or 0 if not found.
  AttrType* previous_attribute(const Ch* name = nullptr, usize name_size = 0,
                               bool CaseInsensitive = true) const {
    if (!name) {
      if (this->m_parent)
        return m_prev_attribute;
      return nullptr;
    }
    if (name_size == 0)
      name_size = internal::measure(name);
    
    for (AttrType* attribute = m_prev_attribute; attribute;
         attribute = attribute->m_prev_attribute) {
      if (internal::compare(attribute->name(), StrRefT(name, name_size),
                            CaseInsensitive))
        return attribute;
    }
    return nullptr;
  }

  //! Gets next attribute, optionally matching attribute name.
  //! \param name Name of attribute to find, or 0 to return next attribute
  //! regardless of its name; this string doesn't have to be zero-terminated if
  //! name_size is non-zero \param name_size Size of name, in characters, or 0
  //! to have size calculated automatically from string \param CaseInsensitive
  //! Should name comparison be case-sensitive; non case-sensitive comparison
  //! works properly only for ASCII characters \return Pointer to found
  //! attribute, or 0 if not found.
  AttrType* next_attribute(const Ch* name = 0, usize name_size = 0,
                                    bool CaseInsensitive = true) const {
    if (name) {
      if (name_size == 0)
        name_size = internal::measure(name);
      for (AttrType* attribute = m_next_attribute; attribute;
           attribute = attribute->m_next_attribute)
        if (internal::compare(attribute->name(), attribute->name_size(), name, name_size,
                              CaseInsensitive))
          return attribute;
      return 0;
    } else
      return this->m_parent ? m_next_attribute : 0;
  }

private:
  AttrType* m_prev_attribute; // Pointer to previous sibling of attribute, or 0 if none; only valid if parent is non-zero
  AttrType* m_next_attribute; // Pointer to next sibling of attribute, or 0 if none; only valid if parent is non-zero
};

///////////////////////////////////////////////////////////////////////////
// XML node

//! Class representing a node of XML document.
//! Each node may have associated name and value strings, which are available
//! through name() and value() functions. Interpretation of name and value
//! depends on type of the node. Type of node can be determined by using type()
//! function. <br><br> Note that after parse, both name and value of node, if
//! any, will point interior of source Text used for parsing. Thus, this Text
//! must persist in the memory for the lifetime of node. \param Ch Character
//! type to use.
template <class Ch = char>
class alignas(kAlignVal) XMLNode : public XMLBase<Ch> {
  RAPIDXML_ALIASES(Ch)
public:
  ///////////////////////////////////////////////////////////////////////////
  // Construction & destruction

  //! Constructs an empty node with the specified type.
  //! Consider using MemoryPool of appropriate document to allocate nodes
  //! manually. \param type Type of node to construct.
  XMLNode(NodeKind type) : m_type(type), m_first_node(0), m_first_attribute(0) {}

  ///////////////////////////////////////////////////////////////////////////
  // Node data access

  //! Gets type of node.
  //! \return Type of node.
  NodeKind type() const { return m_type; }

  ///////////////////////////////////////////////////////////////////////////
  // Related nodes access

  //! Gets document of which node is a child.
  //! \return Pointer to document that contains this node, or 0 if there is no
  //! parent document.
  DocType* document() const {
    NodeType* node = const_cast<NodeType*>(this);
    while (node->parent())
      node = node->parent();
    return node->type() == node_document ? static_cast<DocType*>(node) : 0;
  }

  //! Gets first child node, optionally matching node name.
  //! \param name Name of child to find, or 0 to return first child regardless
  //! of its name; this string doesn't have to be zero-terminated if name_size
  //! is non-zero \param name_size Size of name, in characters, or 0 to have
  //! size calculated automatically from string \param CaseInsensitive Should
  //! name comparison be case-sensitive; non case-sensitive comparison works
  //! properly only for ASCII characters \return Pointer to found child, or 0 if
  //! not found.
  NodeType* first_node(const Ch* name = 0, usize name_size = 0,
                           bool CaseInsensitive = true) const {
    if (!name)
      return m_first_node;
    if (name_size == 0)
      name_size = internal::measure(name);
    for (NodeType* child = m_first_node; child; child = child->next_sibling()) {
      if (internal::compare(child->name(), StrRefT(name, name_size), CaseInsensitive))
        return child;
    }
    return nullptr;
  }

  //! Gets last child node, optionally matching node name.
  //! Behaviour is undefined if node has no children.
  //! Use first_node() to test if node has children.
  //! \param name Name of child to find, or 0 to return last child regardless of
  //! its name; this string doesn't have to be zero-terminated if name_size is
  //! non-zero \param name_size Size of name, in characters, or 0 to have size
  //! calculated automatically from string \param CaseInsensitive Should name
  //! comparison be case-sensitive; non case-sensitive comparison works properly
  //! only for ASCII characters \return Pointer to found child, or 0 if not
  //! found.
  NodeType* last_node(const Ch* name = 0, usize name_size = 0,
                          bool CaseInsensitive = true) const {
    assert(m_first_node); // Cannot query for last child if node has no children
    if (name) {
      if (name_size == 0)
        name_size = internal::measure(name);
      for (NodeType* child = m_last_node; child; child = child->previous_sibling())
        if (internal::compare(child->name(), child->name_size(), name, name_size, CaseInsensitive))
          return child;
      return 0;
    } else
      return m_last_node;
  }

  //! Gets previous sibling node, optionally matching node name.
  //! Behaviour is undefined if node has no parent.
  //! Use parent() to test if node has a parent.
  //! \param name Name of sibling to find, or 0 to return previous sibling
  //! regardless of its name; this string doesn't have to be zero-terminated if
  //! name_size is non-zero \param name_size Size of name, in characters, or 0
  //! to have size calculated automatically from string \param CaseInsensitive
  //! Should name comparison be case-sensitive; non case-sensitive comparison
  //! works properly only for ASCII characters \return Pointer to found sibling,
  //! or 0 if not found.
  NodeType* previous_sibling(const Ch* name = 0, usize name_size = 0,
                                 bool CaseInsensitive = true) const {
    assert(this->m_parent); // Cannot query for siblings if node has no parent
    if (name) {
      if (name_size == 0)
        name_size = internal::measure(name);
      for (NodeType* sibling = m_prev_sibling; sibling; sibling = sibling->m_prev_sibling)
        if (internal::compare(sibling->name(), sibling->name_size(), name, name_size,
                              CaseInsensitive))
          return sibling;
      return 0;
    } else
      return m_prev_sibling;
  }

  //! Gets next sibling node, optionally matching node name.
  //! Behaviour is undefined if node has no parent.
  //! Use parent() to test if node has a parent.
  //! \param name Name of sibling to find, or 0 to return next sibling
  //! regardless of its name; this string doesn't have to be zero-terminated if
  //! name_size is non-zero \param name_size Size of name, in characters, or 0
  //! to have size calculated automatically from string \param CaseInsensitive
  //! Should name comparison be case-sensitive; non case-sensitive comparison
  //! works properly only for ASCII characters \return Pointer to found sibling,
  //! or 0 if not found.
  NodeType* next_sibling(const Ch* name = 0, usize name_size = 0,
                             bool CaseInsensitive = true) const {
    assert(this->m_parent); // Cannot query for siblings if node has no parent
    if (name) {
      if (name_size == 0)
        name_size = internal::measure(name);
      for (NodeType* sibling = m_next_sibling; sibling; sibling = sibling->m_next_sibling) {
        if (internal::compare(sibling->name(), StrRefT(name, name_size),
                              CaseInsensitive))
          return sibling;
      }
      return nullptr;
    } else
      return m_next_sibling;
  }

  //! Gets first attribute of node, optionally matching attribute name.
  //! \param name Name of attribute to find, or 0 to return first attribute
  //! regardless of its name; this string doesn't have to be zero-terminated if
  //! name_size is non-zero \param name_size Size of name, in characters, or 0
  //! to have size calculated automatically from string \param CaseInsensitive
  //! Should name comparison be case-sensitive; non case-sensitive comparison
  //! works properly only for ASCII characters \return Pointer to found
  //! attribute, or 0 if not found.
  AttrType* first_attribute(const Ch* name = 0, usize name_size = 0,
                                     bool CaseInsensitive = true) const {
    if (name) {
      if (name_size == 0)
        name_size = internal::measure(name);
      for (AttrType* attribute = m_first_attribute; attribute;
           attribute = attribute->m_next_attribute) {
        if (internal::compare(attribute->name(), StrRefT(name, name_size),
                              CaseInsensitive))
          return attribute;
      }
      return nullptr;
    } else
      return m_first_attribute;
  }

  //! Gets last attribute of node, optionally matching attribute name.
  //! \param name Name of attribute to find, or 0 to return last attribute
  //! regardless of its name; this string doesn't have to be zero-terminated if
  //! name_size is non-zero \param name_size Size of name, in characters, or 0
  //! to have size calculated automatically from string \param CaseInsensitive
  //! Should name comparison be case-sensitive; non case-sensitive comparison
  //! works properly only for ASCII characters \return Pointer to found
  //! attribute, or 0 if not found.
  AttrType* last_attribute(const Ch* name = 0, usize name_size = 0,
                                    bool CaseInsensitive = true) const {
    if (name) {
      if (name_size == 0)
        name_size = internal::measure(name);
      for (AttrType* attribute = m_last_attribute; attribute;
           attribute = attribute->m_prev_attribute) {
        if (internal::compare(attribute->name(), StrRefT(name, name_size),
                              CaseInsensitive))
          return attribute;
      }
      return nullptr;
    } else
      return m_first_attribute ? m_last_attribute : 0;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Node modification

  //! Sets type of node.
  //! \param type Type of node to set.
  void type(NodeKind type) { m_type = type; }

  ///////////////////////////////////////////////////////////////////////////
  // Node manipulation

  //! Prepends a new child node.
  //! The prepended child becomes the first child, and all existing children are
  //! moved one position back. \param child Node to prepend.
  void prepend_node(NodeType* child) {
    assert(child && !child->parent() && child->type() != node_document);
    if (first_node()) {
      child->m_next_sibling = m_first_node;
      m_first_node->m_prev_sibling = child;
    } else {
      child->m_next_sibling = 0;
      m_last_node = child;
    }
    m_first_node = child;
    child->m_parent = this;
    child->m_prev_sibling = 0;
  }

  //! Appends a new child node.
  //! The appended child becomes the last child.
  //! \param child Node to append.
  void append_node(NodeType* child) {
    assert(child && !child->parent() && child->type() != node_document);
    if (first_node()) {
      child->m_prev_sibling = m_last_node;
      m_last_node->m_next_sibling = child;
    } else {
      child->m_prev_sibling = 0;
      m_first_node = child;
    }
    m_last_node = child;
    child->m_parent = this;
    child->m_next_sibling = 0;
  }

  //! Inserts a new child node at specified place inside the node.
  //! All children after and including the specified node are moved one position
  //! back. \param where Place where to insert the child, or 0 to insert at the
  //! back. \param child Node to insert.
  void insert_node(NodeType* where, NodeType* child) {
    assert(!where || where->parent() == this);
    assert(child && !child->parent() && child->type() != node_document);
    if (where == m_first_node)
      prepend_node(child);
    else if (where == 0)
      append_node(child);
    else {
      child->m_prev_sibling = where->m_prev_sibling;
      child->m_next_sibling = where;
      where->m_prev_sibling->m_next_sibling = child;
      where->m_prev_sibling = child;
      child->m_parent = this;
    }
  }

  //! Removes first child node.
  //! If node has no children, behaviour is undefined.
  //! Use first_node() to test if node has children.
  void remove_first_node() {
    assert(first_node());
    NodeType* child = m_first_node;
    m_first_node = child->m_next_sibling;
    if (child->m_next_sibling)
      child->m_next_sibling->m_prev_sibling = 0;
    else
      m_last_node = 0;
    child->m_parent = 0;
  }

  //! Removes last child of the node.
  //! If node has no children, behaviour is undefined.
  //! Use first_node() to test if node has children.
  void remove_last_node() {
    assert(first_node());
    NodeType* child = m_last_node;
    if (child->m_prev_sibling) {
      m_last_node = child->m_prev_sibling;
      child->m_prev_sibling->m_next_sibling = 0;
    } else
      m_first_node = 0;
    child->m_parent = 0;
  }

  //! Removes specified child from the node
  // \param where Pointer to child to be removed.
  void remove_node(NodeType* where) {
    assert(where && where->parent() == this);
    assert(first_node());
    if (where == m_first_node)
      remove_first_node();
    else if (where == m_last_node)
      remove_last_node();
    else {
      where->m_prev_sibling->m_next_sibling = where->m_next_sibling;
      where->m_next_sibling->m_prev_sibling = where->m_prev_sibling;
      where->m_parent = 0;
    }
  }

  //! Removes all child nodes (but not attributes).
  void remove_all_nodes() {
    for (NodeType* node = first_node(); node; node = node->m_next_sibling)
      node->m_parent = 0;
    m_first_node = 0;
  }

  //! Prepends a new attribute to the node.
  //! \param attribute Attribute to prepend.
  void prepend_attribute(AttrType* attribute) {
    assert(attribute && !attribute->parent());
    if (first_attribute()) {
      attribute->m_next_attribute = m_first_attribute;
      m_first_attribute->m_prev_attribute = attribute;
    } else {
      attribute->m_next_attribute = 0;
      m_last_attribute = attribute;
    }
    m_first_attribute = attribute;
    attribute->m_parent = this;
    attribute->m_prev_attribute = 0;
  }

  //! Appends a new attribute to the node.
  //! \param attribute Attribute to append.
  void append_attribute(AttrType* attribute) {
    assert(attribute && !attribute->parent());
    if (first_attribute()) {
      attribute->m_prev_attribute = m_last_attribute;
      m_last_attribute->m_next_attribute = attribute;
    } else {
      attribute->m_prev_attribute = 0;
      m_first_attribute = attribute;
    }
    m_last_attribute = attribute;
    attribute->m_parent = this;
    attribute->m_next_attribute = 0;
  }

  //! Inserts a new attribute at specified place inside the node.
  //! All attributes after and including the specified attribute are moved one
  //! position back. \param where Place where to insert the attribute, or 0 to
  //! insert at the back. \param attribute Attribute to insert.
  void insert_attribute(AttrType* where, AttrType* attribute) {
    assert(!where || where->parent() == this);
    assert(attribute && !attribute->parent());
    if (where == m_first_attribute)
      prepend_attribute(attribute);
    else if (where == 0)
      append_attribute(attribute);
    else {
      attribute->m_prev_attribute = where->m_prev_attribute;
      attribute->m_next_attribute = where;
      where->m_prev_attribute->m_next_attribute = attribute;
      where->m_prev_attribute = attribute;
      attribute->m_parent = this;
    }
  }

  //! Removes first attribute of the node.
  //! If node has no attributes, behaviour is undefined.
  //! Use first_attribute() to test if node has attributes.
  void remove_first_attribute() {
    assert(first_attribute());
    AttrType* attribute = m_first_attribute;
    if (attribute->m_next_attribute) {
      attribute->m_next_attribute->m_prev_attribute = 0;
    } else
      m_last_attribute = 0;
    attribute->m_parent = 0;
    m_first_attribute = attribute->m_next_attribute;
  }

  //! Removes last attribute of the node.
  //! If node has no attributes, behaviour is undefined.
  //! Use first_attribute() to test if node has attributes.
  void remove_last_attribute() {
    assert(first_attribute());
    AttrType* attribute = m_last_attribute;
    if (attribute->m_prev_attribute) {
      attribute->m_prev_attribute->m_next_attribute = 0;
      m_last_attribute = attribute->m_prev_attribute;
    } else
      m_first_attribute = 0;
    attribute->m_parent = 0;
  }

  //! Removes specified attribute from node.
  //! \param where Pointer to attribute to be removed.
  void remove_attribute(AttrType* where) {
    assert(first_attribute() && where->parent() == this);
    if (where == m_first_attribute)
      remove_first_attribute();
    else if (where == m_last_attribute)
      remove_last_attribute();
    else {
      where->m_prev_attribute->m_next_attribute = where->m_next_attribute;
      where->m_next_attribute->m_prev_attribute = where->m_prev_attribute;
      where->m_parent = 0;
    }
  }

  //! Removes all attributes of node.
  void remove_all_attributes() {
    for (AttrType* attribute = first_attribute(); attribute;
         attribute = attribute->m_next_attribute)
      attribute->m_parent = 0;
    m_first_attribute = 0;
  }

private:
  ///////////////////////////////////////////////////////////////////////////
  // Restrictions

  // No copying
  XMLNode(const XMLNode&);
  void operator=(const XMLNode&);

  ///////////////////////////////////////////////////////////////////////////
  // Data members

  // Note that some of the pointers below have UNDEFINED values if certain other
  // pointers are 0. This is required for maximum performance, as it allows the
  // parser to omit initialization of unneded/redundant values.
  //
  // The rules are as follows:
  // 1. first_node and first_attribute contain valid pointers, or 0 if node has
  // no children/attributes respectively
  // 2. last_node and last_attribute are valid only if node has at least one
  // child/attribute respectively, otherwise they contain garbage
  // 3. prev_sibling and next_sibling are valid only if node has a parent,
  // otherwise they contain garbage

  NodeKind m_type;           // Type of node; always valid
  NodeType* m_first_node; // Pointer to first child node, or 0 if none; always valid
  NodeType* m_last_node;  // Pointer to last child node, or 0 if none; this value is only valid if m_first_node is non-zero
  AttrType* m_first_attribute; // Pointer to first attribute of node, or 0 if none; always valid
  AttrType* m_last_attribute; // Pointer to last attribute of node, or 0 if none; this value is only valid if m_first_attribute is non-zero
  NodeType* m_prev_sibling; // Pointer to previous sibling of node, or 0 if none; this value is only valid if m_parent is non-zero
  NodeType* m_next_sibling; // Pointer to next sibling of node, or 0 if none; this value is only valid if m_parent is non-zero
};

///////////////////////////////////////////////////////////////////////////
// XML document

//! This class represents root of the DOM hierarchy.
//! It is also an XMLNode and a MemoryPool through public inheritance.
//! Use parse() function to build a DOM tree from a zero-terminated XML Text
//! string. parse() function allocates memory for nodes and attributes by using
//! functions of XMLDocument, which are inherited from MemoryPool. To access
//! root node of the document, use the document itself, as if it was an
//! XMLNode. \param Ch Character type to use.
template <class Ch = char>
class alignas(kAlignVal) XMLDocument
                : public XMLNode<Ch>, public MemoryPool<Ch> {
  RAPIDXML_ALIASES(Ch)
public:
  //! Constructs empty XML document
  XMLDocument() : NodeType(node_document), MemoryPool<Ch>() {}
  //! Constructs pool from input allocator.
  explicit XMLDocument(XMLBumpAllocator& A EXI_LIFETIMEBOUND) :
   NodeType(node_document), MemoryPool<Ch>(A) {
  }

  //! Parses zero-terminated XML string according to given flags.
  //! Passed string will be modified by the parser, unless
  //! xml::parse_non_destructive flag is used. The string must persist for
  //! the lifetime of the document. In case of error, xml::parse_error
  //! exception will be thrown. <br><br> If you want to parse contents of a
  //! file, you must first load the file into the memory, and pass pointer to
  //! its beginning. Make sure that data is zero-terminated. <br><br> Document
  //! can be parsed into multiple times. Each new call to parse removes previous
  //! nodes and attributes (if any), but does not clear memory pool. \param Text
  //! XML data to parse; pointer is non-const to denote fact that this data may
  //! be modified by the parser.
  template <int Flags> void parse(Ch* Text) {
    assert(Text);

    // Remove current contents
    this->remove_all_nodes();
    this->remove_all_attributes();

    // Parse BOM, if any
    parse_bom<Flags>(Text);

    // Parse children
    while (1) {
      // Skip whitespace before node
      skip<whitespace_pred, Flags>(Text);
      if (*Text == 0)
        break;

      // Parse and append new child
      if (*Text == Ch('<')) {
        ++Text; // Skip '<'
        if (NodeType* node = parse_node<Flags>(Text))
          this->append_node(node);
      } else
        RAPIDXML_PARSE_ERROR("expected <", Text);
    }
  }

  //! Clears the document by deleting all nodes and clearing the memory pool.
  //! All nodes owned by document pool are destroyed.
  void clear() {
    this->remove_all_nodes();
    this->remove_all_attributes();
    MemoryPool<Ch>::clear();
  }

private:
  ///////////////////////////////////////////////////////////////////////
  // Internal character utility functions

  // Detect whitespace character
  struct whitespace_pred {
    static u8 test(Ch ch) {
      return internal::LookupTables<0>::whitespace[u8(ch)];
    }
  };

  // Detect node name character
  struct node_name_pred {
    static u8 test(Ch ch) {
      return internal::LookupTables<0>::node_name[u8(ch)];
    }
  };

  // Detect attribute name character
  struct attribute_name_pred {
    static u8 test(Ch ch) {
      return internal::LookupTables<0>::attribute_name[u8(ch)];
    }
  };

  // Detect Text character (PCDATA)
  struct text_pred {
    static u8 test(Ch ch) {
      return internal::LookupTables<0>::Text[u8(ch)];
    }
  };

  // Detect Text character (PCDATA) that does not require processing
  struct text_pure_no_ws_pred {
    static u8 test(Ch ch) {
      return internal::LookupTables<0>::text_pure_no_ws[u8(ch)];
    }
  };

  // Detect Text character (PCDATA) that does not require processing
  struct text_pure_with_ws_pred {
    static u8 test(Ch ch) {
      return internal::LookupTables<0>::text_pure_with_ws[u8(ch)];
    }
  };

  // Detect attribute value character
  template <Ch Quote> struct attribute_value_pred {
    static u8 test(Ch ch) {
      if (Quote == Ch('\''))
        return internal::LookupTables<0>::attribute_data_1[u8(ch)];
      if (Quote == Ch('\"'))
        return internal::LookupTables<0>::attribute_data_2[u8(ch)];
      return 0; // Should never be executed, to avoid warnings on Comeau
    }
  };

  // Detect attribute value character
  template <Ch Quote> struct attribute_value_pure_pred {
    static u8 test(Ch ch) {
      if (Quote == Ch('\''))
        return internal::LookupTables<0>::attribute_data_1_pure[u8(ch)];
      if (Quote == Ch('\"'))
        return internal::LookupTables<0>::attribute_data_2_pure[u8(ch)];
      return 0; // Should never be executed, to avoid warnings on Comeau
    }
  };

  // Insert coded character, using UTF8 or 8-bit ASCII
  template <int Flags> static void insert_coded_character(Ch*& Text, unsigned long Code) {
    if (Flags & parse_no_utf8) {
      // Insert 8-bit ASCII character
      // Todo: possibly verify that Code is less than 256 and use replacement
      // char otherwise?
      Text[0] = u8(Code);
      Text += 1;
    } else {
      // Insert UTF8 sequence
      if (Code < 0x80) // 1 byte sequence
      {
        Text[0] = u8(Code);
        Text += 1;
      } else if (Code < 0x800) // 2 byte sequence
      {
        Text[1] = u8((Code | 0x80) & 0xBF);
        Code >>= 6;
        Text[0] = u8(Code | 0xC0);
        Text += 2;
      } else if (Code < 0x10000) // 3 byte sequence
      {
        Text[2] = u8((Code | 0x80) & 0xBF);
        Code >>= 6;
        Text[1] = u8((Code | 0x80) & 0xBF);
        Code >>= 6;
        Text[0] = u8(Code | 0xE0);
        Text += 3;
      } else if (Code < 0x110000) // 4 byte sequence
      {
        Text[3] = u8((Code | 0x80) & 0xBF);
        Code >>= 6;
        Text[2] = u8((Code | 0x80) & 0xBF);
        Code >>= 6;
        Text[1] = u8((Code | 0x80) & 0xBF);
        Code >>= 6;
        Text[0] = u8(Code | 0xF0);
        Text += 4;
      } else // Invalid, only codes up to 0x10FFFF are allowed in Unicode
      {
        RAPIDXML_PARSE_ERROR("invalid numeric character entity", Text);
      }
    }
  }

  // Skip characters until predicate evaluates to true
  template <class StopPred, int Flags> static void skip(Ch*& Text) {
    Ch* tmp = Text;
    while (StopPred::test(*tmp))
      ++tmp;
    Text = tmp;
  }

  // Skip characters until predicate evaluates to true while doing the
  // following:
  // - replacing XML character entity references with proper characters (&apos;
  // &amp; &quot; &lt; &gt; &#...;)
  // - condensing whitespace sequences to single space character
  template <class StopPred, class StopPredPure, int Flags>
  static Ch* skip_and_expand_character_refs(Ch*& Text) {
    // If entity translation, whitespace condense and whitespace trimming is
    // disabled, use plain skip
    if (Flags & parse_no_entity_translation && !(Flags & parse_normalize_whitespace) &&
        !(Flags & parse_trim_whitespace)) {
      skip<StopPred, Flags>(Text);
      return Text;
    }

    // Use simple skip until first modification is detected
    skip<StopPredPure, Flags>(Text);

    // Use translation skip
    Ch* src = Text;
    Ch* dest = src;
    while (StopPred::test(*src)) {
      // If entity translation is enabled
      if (!(Flags & parse_no_entity_translation)) {
        // Test if replacement is needed
        if (src[0] == Ch('&')) {
          switch (src[1]) {

          // &amp; &apos;
          case Ch('a'):
            if (src[2] == Ch('m') && src[3] == Ch('p') && src[4] == Ch(';')) {
              *dest = Ch('&');
              ++dest;
              src += 5;
              continue;
            }
            if (src[2] == Ch('p') && src[3] == Ch('o') && src[4] == Ch('s') && src[5] == Ch(';')) {
              *dest = Ch('\'');
              ++dest;
              src += 6;
              continue;
            }
            break;

          // &quot;
          case Ch('q'):
            if (src[2] == Ch('u') && src[3] == Ch('o') && src[4] == Ch('t') && src[5] == Ch(';')) {
              *dest = Ch('"');
              ++dest;
              src += 6;
              continue;
            }
            break;

          // &gt;
          case Ch('g'):
            if (src[2] == Ch('t') && src[3] == Ch(';')) {
              *dest = Ch('>');
              ++dest;
              src += 4;
              continue;
            }
            break;

          // &lt;
          case Ch('l'):
            if (src[2] == Ch('t') && src[3] == Ch(';')) {
              *dest = Ch('<');
              ++dest;
              src += 4;
              continue;
            }
            break;

          // &#...; - assumes ASCII
          case Ch('#'):
            if (src[2] == Ch('x')) {
              unsigned long Code = 0;
              src += 3; // Skip &#x
              while (1) {
                u8 digit =
                    internal::LookupTables<0>::digits[u8(*src)];
                if (digit == 0xFF)
                  break;
                Code = Code * 16 + digit;
                ++src;
              }
              insert_coded_character<Flags>(dest,
                                            Code); // Put character in output
            } else {
              unsigned long Code = 0;
              src += 2; // Skip &#
              while (1) {
                u8 digit =
                    internal::LookupTables<0>::digits[u8(*src)];
                if (digit == 0xFF)
                  break;
                Code = Code * 10 + digit;
                ++src;
              }
              insert_coded_character<Flags>(dest,
                                            Code); // Put character in output
            }
            if (*src == Ch(';'))
              ++src;
            else
              RAPIDXML_PARSE_ERROR("expected ;", src);
            continue;

          // Something else
          default:
            // Ignore, just copy '&' verbatim
            break;
          }
        }
      }

      // If whitespace condensing is enabled
      if (Flags & parse_normalize_whitespace) {
        // Test if condensing is needed
        if (whitespace_pred::test(*src)) {
          *dest = Ch(' ');
          ++dest; // Put single space in dest
          ++src;  // Skip first whitespace char
          // Skip remaining whitespace chars
          while (whitespace_pred::test(*src))
            ++src;
          continue;
        }
      }

      // No replacement, only copy character
      *dest++ = *src++;
    }

    // Return new end
    Text = src;
    return dest;
  }

  ///////////////////////////////////////////////////////////////////////
  // Internal parsing functions

  // Parse BOM, if any
  template <int Flags> void parse_bom(Ch*& Text) {
    // UTF-8?
    if (u8(Text[0]) == 0xEF && u8(Text[1]) == 0xBB &&
        u8(Text[2]) == 0xBF) {
      Text += 3; // Skup utf-8 bom
    }
  }

  // Parse XML declaration (<?xml...)
  template <int Flags> NodeType* parse_xml_declaration(Ch*& Text) {
    // If parsing of declaration is disabled
    if (!(Flags & parse_declaration_node)) {
      // Skip until end of declaration
      while (Text[0] != Ch('?') || Text[1] != Ch('>')) {
        if (!Text[0])
          RAPIDXML_PARSE_ERROR("unexpected end of data", Text);
        ++Text;
      }
      Text += 2; // Skip '?>'
      return 0;
    }

    // Create declaration
    NodeType* Decl = this->allocate_node(node_declaration);

    // Skip whitespace before attributes or ?>
    skip<whitespace_pred, Flags>(Text);

    // Parse declaration attributes
    parse_node_attributes<Flags>(Text, Decl);

    // Skip ?>
    if (Text[0] != Ch('?') || Text[1] != Ch('>'))
      RAPIDXML_PARSE_ERROR("expected ?>", Text);
    Text += 2;

    return Decl;
  }

  // Parse XML comment (<!--...)
  template <int Flags> NodeType* parse_comment(Ch*& Text) {
    // If parsing of comments is disabled
    if (!(Flags & parse_comment_nodes)) {
      // Skip until end of comment
      while (Text[0] != Ch('-') || Text[1] != Ch('-') || Text[2] != Ch('>')) {
        if (!Text[0])
          RAPIDXML_PARSE_ERROR("unexpected end of data", Text);
        ++Text;
      }
      Text += 3; // Skip '-->'
      return 0;  // Do not produce comment node
    }

    // Remember value start
    Ch* value = Text;

    // Skip until end of comment
    while (Text[0] != Ch('-') || Text[1] != Ch('-') || Text[2] != Ch('>')) {
      if (!Text[0])
        RAPIDXML_PARSE_ERROR("unexpected end of data", Text);
      ++Text;
    }

    // Create comment node
    NodeType* comment = this->allocate_node(node_comment);
    comment->value(value, Text - value);

    // Place zero terminator after comment value
    if (!(Flags & parse_no_string_terminators))
      *Text = Ch('\0');

    Text += 3; // Skip '-->'
    return comment;
  }

  // Parse DOCTYPE
  template <int Flags> NodeType* parse_doctype(Ch*& Text) {
    // Remember value start
    Ch* value = Text;

    // Skip to >
    while (*Text != Ch('>')) {
      // Determine character type
      switch (*Text) {

      // If '[' encountered, scan for matching ending ']' using naive algorithm
      // with depth This works for all W3C test files except for 2 most wicked
      case Ch('['): {
        ++Text; // Skip '['
        int depth = 1;
        while (depth > 0) {
          switch (*Text) {
          case Ch('['): ++depth; break;
          case Ch(']'): --depth; break;
          case 0: RAPIDXML_PARSE_ERROR("unexpected end of data", Text);
          }
          ++Text;
        }
        break;
      }

      // Error on end of Text
      case Ch('\0'): RAPIDXML_PARSE_ERROR("unexpected end of data", Text);

      // Other character, skip it
      default: ++Text;
      }
    }

    // If DOCTYPE nodes enabled
    if (Flags & parse_doctype_node) {
      // Create a new doctype node
      NodeType* doctype = this->allocate_node(node_doctype);
      doctype->value(value, Text - value);

      // Place zero terminator after value
      if (!(Flags & parse_no_string_terminators))
        *Text = Ch('\0');

      Text += 1; // skip '>'
      return doctype;
    } else {
      Text += 1; // skip '>'
      return 0;
    }
  }

  // Parse PI
  template <int Flags> NodeType* parse_pi(Ch*& Text) {
    // If creation of PI nodes is enabled
    if (Flags & parse_pi_nodes) {
      // Create pi node
      NodeType* pi = this->allocate_node(node_pi);

      // Extract PI target name
      Ch* name = Text;
      skip<node_name_pred, Flags>(Text);
      if (Text == name)
        RAPIDXML_PARSE_ERROR("expected PI target", Text);
      pi->name(name, Text - name);

      // Skip whitespace between pi target and pi
      skip<whitespace_pred, Flags>(Text);

      // Remember start of pi
      Ch* value = Text;

      // Skip to '?>'
      while (Text[0] != Ch('?') || Text[1] != Ch('>')) {
        if (*Text == Ch('\0'))
          RAPIDXML_PARSE_ERROR("unexpected end of data", Text);
        ++Text;
      }

      // Set pi value (verbatim, no entity expansion or whitespace
      // normalization)
      pi->value(value, Text - value);

      // Place zero terminator after name and value
      if (!(Flags & parse_no_string_terminators)) {
        pi->name_data()[pi->name_size()] = Ch('\0');
        pi->value_data()[pi->value_size()] = Ch('\0');
      }

      Text += 2; // Skip '?>'
      return pi;
    } else {
      // Skip to '?>'
      while (Text[0] != Ch('?') || Text[1] != Ch('>')) {
        if (*Text == Ch('\0'))
          RAPIDXML_PARSE_ERROR("unexpected end of data", Text);
        ++Text;
      }
      Text += 2; // Skip '?>'
      return 0;
    }
  }

  // Parse and append data
  // Return character that ends data.
  // This is necessary because this character might have been overwritten by a
  // terminating 0
  template <int Flags>
  Ch parse_and_append_data(NodeType* node, Ch*& Text, Ch* contents_start) {
    // Backup to contents start if whitespace trimming is disabled
    if (!(Flags & parse_trim_whitespace))
      Text = contents_start;

    // Skip until end of data
    Ch *value = Text, *end;
    if (Flags & parse_normalize_whitespace)
      end = skip_and_expand_character_refs<text_pred, text_pure_with_ws_pred, Flags>(Text);
    else
      end = skip_and_expand_character_refs<text_pred, text_pure_no_ws_pred, Flags>(Text);

    // Trim trailing whitespace if flag is set; leading was already trimmed by
    // whitespace skip after >
    if (Flags & parse_trim_whitespace) {
      if (Flags & parse_normalize_whitespace) {
        // Whitespace is already condensed to single space characters by
        // skipping function, so just trim 1 char off the end
        if (*(end - 1) == Ch(' '))
          --end;
      } else {
        // Backup until non-whitespace character is found
        while (whitespace_pred::test(*(end - 1)))
          --end;
      }
    }

    // If characters are still left between end and value (this test is only
    // necessary if normalization is enabled) Create new data node
    if (!(Flags & parse_no_data_nodes)) {
      NodeType* data = this->allocate_node(node_data);
      data->value(value, end - value);
      node->append_node(data);
    }

    // Add data to parent node if no data exists yet
    if (!(Flags & parse_no_element_values))
      if (*node->value_data() == Ch('\0'))
        node->value(value, end - value);

    // Place zero terminator after value
    if (!(Flags & parse_no_string_terminators)) {
      Ch ch = *Text;
      *end = Ch('\0');
      return ch; // Return character that ends data; this is required because
                 // zero terminator overwritten it
    }

    // Return character that ends data
    return *Text;
  }

  // Parse CDATA
  template <int Flags> NodeType* parse_cdata(Ch*& Text) {
    // If CDATA is disabled
    if (Flags & parse_no_data_nodes) {
      // Skip until end of cdata
      while (Text[0] != Ch(']') || Text[1] != Ch(']') || Text[2] != Ch('>')) {
        if (!Text[0])
          RAPIDXML_PARSE_ERROR("unexpected end of data", Text);
        ++Text;
      }
      Text += 3; // Skip ]]>
      return 0;  // Do not produce CDATA node
    }

    // Skip until end of cdata
    Ch* value = Text;
    while (Text[0] != Ch(']') || Text[1] != Ch(']') || Text[2] != Ch('>')) {
      if (!Text[0])
        RAPIDXML_PARSE_ERROR("unexpected end of data", Text);
      ++Text;
    }

    // Create new cdata node
    NodeType* cdata = this->allocate_node(node_cdata);
    cdata->value(value, Text - value);

    // Place zero terminator after value
    if (!(Flags & parse_no_string_terminators))
      *Text = Ch('\0');

    Text += 3; // Skip ]]>
    return cdata;
  }

  // Parse element node
  template <int Flags> NodeType* parse_element(Ch*& Text) {
    // Create element node
    NodeType* element = this->allocate_node(node_element);

    // Extract element name
    Ch* name = Text;
    skip<node_name_pred, Flags>(Text);
    if (Text == name)
      RAPIDXML_PARSE_ERROR("expected element name", Text);
    element->name(name, Text - name);

    // Skip whitespace between element name and attributes or >
    skip<whitespace_pred, Flags>(Text);

    // Parse attributes, if any
    parse_node_attributes<Flags>(Text, element);

    // Determine ending type
    if (*Text == Ch('>')) {
      ++Text;
      parse_node_contents<Flags>(Text, element);
    } else if (*Text == Ch('/')) {
      ++Text;
      if (*Text != Ch('>'))
        RAPIDXML_PARSE_ERROR("expected >", Text);
      ++Text;
    } else
      RAPIDXML_PARSE_ERROR("expected >", Text);

    // Place zero terminator after name
    if (!(Flags & parse_no_string_terminators))
      element->name_data()[element->name_size()] = Ch('\0');

    // Return parsed element
    return element;
  }

  // Determine node type, and parse it
  template <int Flags> NodeType* parse_node(Ch*& Text) {
    // Parse proper node type
    switch (Text[0]) {

    // <...
    default:
      // Parse and append element node
      return parse_element<Flags>(Text);

    // <?...
    case Ch('?'):
      ++Text; // Skip ?
      if ((Text[0] == Ch('x') || Text[0] == Ch('X')) &&
          (Text[1] == Ch('m') || Text[1] == Ch('M')) &&
          (Text[2] == Ch('l') || Text[2] == Ch('L')) && whitespace_pred::test(Text[3])) {
        // '<?xml ' - xml declaration
        Text += 4; // Skip 'xml '
        return parse_xml_declaration<Flags>(Text);
      } else {
        // Parse PI
        return parse_pi<Flags>(Text);
      }

    // <!...
    case Ch('!'):

      // Parse proper subset of <! node
      switch (Text[1]) {

      // <!-
      case Ch('-'):
        if (Text[2] == Ch('-')) {
          // '<!--' - xml comment
          Text += 3; // Skip '!--'
          return parse_comment<Flags>(Text);
        }
        break;

      // <![
      case Ch('['):
        if (Text[2] == Ch('C') && Text[3] == Ch('D') && Text[4] == Ch('A') &&
            Text[5] == Ch('T') && Text[6] == Ch('A') && Text[7] == Ch('[')) {
          // '<![CDATA[' - cdata
          Text += 8; // Skip '![CDATA['
          return parse_cdata<Flags>(Text);
        }
        break;

      // <!D
      case Ch('D'):
        if (Text[2] == Ch('O') && Text[3] == Ch('C') && Text[4] == Ch('T') && Text[5] == Ch('Y') &&
            Text[6] == Ch('P') && Text[7] == Ch('E') && whitespace_pred::test(Text[8])) {
          // '<!DOCTYPE ' - doctype
          Text += 9; // skip '!DOCTYPE '
          return parse_doctype<Flags>(Text);
        }

      } // switch

      // Attempt to skip other, unrecognized node types starting with <!
      ++Text; // Skip !
      while (*Text != Ch('>')) {
        if (*Text == 0)
          RAPIDXML_PARSE_ERROR("unexpected end of data", Text);
        ++Text;
      }
      ++Text;   // Skip '>'
      return 0; // No node recognized
    }
  }

  // Parse contents of the node - children, data etc.
  template <int Flags> void parse_node_contents(Ch*& Text, NodeType* node) {
    // For all children and Text
    while (1) {
      // Skip whitespace between > and node contents
      Ch* contents_start = Text; // Store start of node contents before whitespace is skipped
      skip<whitespace_pred, Flags>(Text);
      Ch next_char = *Text;

    // After data nodes, instead of continuing the loop, control jumps here.
    // This is because zero termination inside parse_and_append_data() function
    // would wreak havoc with the above Code.
    // Also, skipping whitespace after data nodes is unnecessary.
    after_data_node:

      // Determine what comes next: node closing, child node, data node, or 0?
      switch (next_char) {

      // Node closing or child node
      case Ch('<'):
        if (Text[1] == Ch('/')) {
          // Node closing
          Text += 2; // Skip '</'
          if (Flags & parse_validate_closing_tags) {
            // Skip and validate closing tag name
            Ch* closing_name = Text;
            skip<node_name_pred, Flags>(Text);
            if (!internal::compare(node->name(),
                StrRefT(closing_name, Text - closing_name), true))
              RAPIDXML_PARSE_ERROR("invalid closing tag name", Text);
          } else {
            // No validation, just skip name
            skip<node_name_pred, Flags>(Text);
          }
          // Skip remaining whitespace after node name
          skip<whitespace_pred, Flags>(Text);
          if (*Text != Ch('>'))
            RAPIDXML_PARSE_ERROR("expected >", Text);
          ++Text; // Skip '>'
          return; // Node closed, finished parsing contents
        } else {
          // Child node
          ++Text; // Skip '<'
          if (NodeType* child = parse_node<Flags>(Text))
            node->append_node(child);
        }
        break;

      // End of data - error
      case Ch('\0'): RAPIDXML_PARSE_ERROR("unexpected end of data", Text);

      // Data node
      default:
        next_char = parse_and_append_data<Flags>(node, Text, contents_start);
        goto after_data_node; // Bypass regular processing after data nodes
      }
    }
  }

  // Parse XML attributes of the node
  template <int Flags> void parse_node_attributes(Ch*& Text, NodeType* node) {
    // For all attributes
    while (attribute_name_pred::test(*Text)) {
      // Extract attribute name
      Ch* name = Text;
      ++Text; // Skip first character of attribute name
      skip<attribute_name_pred, Flags>(Text);
      if (Text == name)
        RAPIDXML_PARSE_ERROR("expected attribute name", name);

      // Create new attribute
      AttrType* attribute = this->allocate_attribute();
      attribute->name(name, Text - name);
      node->append_attribute(attribute);

      // Skip whitespace after attribute name
      skip<whitespace_pred, Flags>(Text);

      // Skip =
      if (*Text != Ch('='))
        RAPIDXML_PARSE_ERROR("expected =", Text);
      ++Text;

      // Add terminating zero after name
      if (!(Flags & parse_no_string_terminators))
        attribute->name_data()[attribute->name_size()] = 0;

      // Skip whitespace after =
      skip<whitespace_pred, Flags>(Text);

      // Skip quote and remember if it was ' or "
      Ch quote = *Text;
      if (quote != Ch('\'') && quote != Ch('"'))
        RAPIDXML_PARSE_ERROR("expected ' or \"", Text);
      ++Text;

      // Extract attribute value and expand char refs in it
      Ch *value = Text, *end;
      const int AttFlags = Flags & ~parse_normalize_whitespace; // No whitespace normalization
                                                                // in attributes
      if (quote == Ch('\''))
        end = skip_and_expand_character_refs<attribute_value_pred<Ch('\'')>,
                                             attribute_value_pure_pred<Ch('\'')>, AttFlags>(Text);
      else
        end = skip_and_expand_character_refs<attribute_value_pred<Ch('"')>,
                                             attribute_value_pure_pred<Ch('"')>, AttFlags>(Text);

      // Set attribute value
      attribute->value(value, end - value);

      // Make sure that end quote is present
      if (*Text != quote)
        RAPIDXML_PARSE_ERROR("expected ' or \"", Text);
      ++Text; // Skip quote

      // Add terminating zero after value
      if (!(Flags & parse_no_string_terminators))
        attribute->value_data()[attribute->value_size()] = 0;

      // Skip whitespace after attribute value
      skip<whitespace_pred, Flags>(Text);
    }
  }
};

//! \cond internal
namespace internal {

  // Whitespace (space \n \r \t)
  template <int Dummy>
  inline constexpr u8 LookupTables<Dummy>::whitespace[256] = {
  // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
     0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  0,  0,  1,  0,  0,  // 0
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 1
     1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 2
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 3
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 4
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 5
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 6
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 7
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 8
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 9
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // A
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // B
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // C
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // D
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // E
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   // F
  };

  // Node name (anything but space \n \r \t / > ? \0)
  template <int Dummy>
  inline constexpr u8 LookupTables<Dummy>::node_name[256] = {
  // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
     0,  1,  1,  1,  1,  1,  1,  1,  1,  0,  0,  1,  1,  0,  1,  1,  // 0
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 1
     0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  // 2
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  0,  // 3
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 4
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 5
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 6
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 7
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 8
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 9
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // A
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // B
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // C
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // D
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // E
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1   // F
  };

  // Text (i.e. PCDATA) (anything but < \0)
  template <int Dummy>
  inline constexpr u8 LookupTables<Dummy>::Text[256] = {
  // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
     0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 0
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 1
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 2
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  1,  1,  1,  // 3
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 4
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 5
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 6
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 7
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 8
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 9
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // A
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // B
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // C
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // D
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // E
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1   // F
  };

  // Text (i.e. PCDATA) that does not require processing when ws normalization is disabled 
  // (anything but < \0 &)
  template <int Dummy>
  inline constexpr u8 LookupTables<Dummy>::text_pure_no_ws[256] = {
  // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
     0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 0
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 1
     1,  1,  1,  1,  1,  1,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 2
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  1,  1,  1,  // 3
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 4
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 5
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 6
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 7
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 8
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 9
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // A
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // B
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // C
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // D
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // E
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1   // F
  };

  // Text (i.e. PCDATA) that does not require processing when ws normalizationis is enabled
  // (anything but < \0 & space \n \r \t)
  template <int Dummy>
  inline constexpr u8 LookupTables<Dummy>::text_pure_with_ws[256] = {
  // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
     0,  1,  1,  1,  1,  1,  1,  1,  1,  0,  0,  1,  1,  0,  1,  1,  // 0
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 1
     0,  1,  1,  1,  1,  1,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 2
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  1,  1,  1,  // 3
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 4
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 5
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 6
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 7
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 8
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 9
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // A
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // B
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // C
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // D
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // E
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1   // F
  };

  // Attribute name (anything but space \n \r \t / < > = ? ! \0)
  template <int Dummy>
  inline constexpr u8 LookupTables<Dummy>::attribute_name[256] = {
  // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
     0,  1,  1,  1,  1,  1,  1,  1,  1,  0,  0,  1,  1,  0,  1,  1,  // 0
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 1
     0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  // 2
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  0,  0,  0,  // 3
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 4
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 5
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 6
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 7
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 8
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 9
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // A
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // B
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // C
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // D
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // E
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1   // F
  };

  // Attribute data with single quote (anything but ' \0)
  template <int Dummy>
  inline constexpr u8 LookupTables<Dummy>::attribute_data_1[256] = {
  // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
     0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 0
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 1
     1,  1,  1,  1,  1,  1,  1,  0,  1,  1,  1,  1,  1,  1,  1,  1,  // 2
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 3
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 4
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 5
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 6
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 7
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 8
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 9
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // A
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // B
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // C
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // D
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // E
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1   // F
  };

  // Attribute data with single quote that does not require processing (anything but ' \0 &)
  template <int Dummy>
  inline constexpr u8 LookupTables<Dummy>::attribute_data_1_pure[256] = {
  // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
     0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 0
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 1
     1,  1,  1,  1,  1,  1,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  // 2
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 3
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 4
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 5
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 6
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 7
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 8
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 9
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // A
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // B
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // C
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // D
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // E
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1   // F
  };

  // Attribute data with double quote (anything but " \0)
  template <int Dummy>
  inline constexpr u8 LookupTables<Dummy>::attribute_data_2[256] = {
  // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
     0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 0
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 1
     1,  1,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 2
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 3
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 4
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 5
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 6
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 7
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 8
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 9
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // A
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // B
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // C
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // D
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // E
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1   // F
  };

  // Attribute data with double quote that does not require processing (anything but " \0 &)
  template <int Dummy>
  inline constexpr u8 LookupTables<Dummy>::attribute_data_2_pure[256] = {
  // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
     0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 0
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 1
     1,  1,  0,  1,  1,  1,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 2
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 3
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 4
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 5
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 6
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 7
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 8
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 9
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // A
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // B
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // C
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // D
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // E
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1   // F
  };

  // Digits (dec and hex, 255 denotes end of numeric character reference)
  template <int Dummy>
  inline constexpr u8 LookupTables<Dummy>::digits[256] = {
  // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
     255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // 0
     255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // 1
     255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // 2
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9,255,255,255,255,255,255,  // 3
     255, 10, 11, 12, 13, 14, 15,255,255,255,255,255,255,255,255,255,  // 4
     255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // 5
     255, 10, 11, 12, 13, 14, 15,255,255,255,255,255,255,255,255,255,  // 6
     255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // 7
     255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // 8
     255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // 9
     255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // A
     255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // B
     255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // C
     255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // D
     255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // E
     255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255   // F
  };

  // Upper case conversion
  template <int Dummy>
  inline constexpr u8 LookupTables<Dummy>::upcase[256] = {
  // 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  A   B   C   D   E   F
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,   // 0
     16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,   // 1
     32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,   // 2
     48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,   // 3
     64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,   // 4
     80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,   // 5
     96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,   // 6
     80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 123,124,125,126,127,  // 7
     128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,  // 8
     144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,  // 9
     160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,  // A
     176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,  // B
     192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,  // C
     208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,  // D
     224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,  // E
     240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255   // F
  };
} // namespace internal
//! \endcond

} // namespace xml

// Undefine internal macros
#undef RAPIDXML_PARSE_ERROR

// On MSVC, restore warnings state
#ifdef _MSC_VER
# pragma warning(pop)
#endif

#endif
