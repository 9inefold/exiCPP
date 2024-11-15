//===- exicpp/Debug/Format.hpp --------------------------------------===//
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

#include "_Format.hpp"
#include "Terminal.hpp"

#undef EXICPP_LOG_FUNC
#if !NFORMAT
# define EXICPP_LOG_FUNC(...) \
  ::exi::dbg::logInternal<true>(__VA_ARGS__)
#else
# define EXICPP_LOG_FUNC(...) ((void)0)
#endif

#undef LOG_ASSERT
#ifndef NDEBUG
# define LOG_ASSERT(expr) \
  (EXICPP_EXPECT_TRUE(static_cast<bool>(expr)) ? ((void)0) \
    : LOG_FATAL("Assertion failed: {}", #expr))
#else
# define LOG_ASSERT(expr) ((void)0)
#endif

//======================================================================//
// Persistent
//======================================================================//

#ifndef EXICPP_DEBUG_FORMAT_HPP
#define EXICPP_DEBUG_FORMAT_HPP

#if EXICPP_DEBUG
# define LOG_INTERNAL(...) EXICPP_LOG_FUNC(__VA_ARGS__)
#else
# define LOG_INTERNAL(...) ((void)0)
#endif

#define LOG(dbglevel, ...) \
  ((dbglevel >= EXICPP_DEBUG_LEVEL) \
    ? LOG_INTERNAL(EXICPP_FMT_LOC(), \
        fmt::format(__VA_ARGS__), dbglevel) \
    : (void(0)))

#define LOG_INFO(...)   LOG(INFO,     __VA_ARGS__)
#define LOG_WARN(...)   LOG(WARNING,  __VA_ARGS__)
#define LOG_ERROR(...)  LOG(ERROR,    __VA_ARGS__)
#define LOG_FATAL(...)  \
  EXICPP_LOG_FUNC(EXICPP_FMT_LOC(), \
    fmt::format(__VA_ARGS__), FATAL)

#define LOG_ERRCODE(err_code) \
  LOG_INTERNAL(EXICPP_FMT_LOC(), \
    GET_ERR_STRING(CErrCode(err_code)), ERROR)

#if EXICPP_ANSI
# define EXICPP_STYLED(...) (::exi::dbg::styled(__VA_ARGS__))
#else
# define EXICPP_STYLED(...) (__VA_ARGS__)
#endif

namespace exi {
namespace dbg {

ALWAYS_INLINE bool has_ansi() {
#if EXICPP_ANSI
  return can_use_ansi();
#else
  return false;
#endif
}

#if EXICPP_ANSI
template <typename T>
struct Styled : public fmt::detail::styled_arg<T> {
  using Base = fmt::detail::styled_arg<T>;
  using Base::Base;
};

template <typename T>
FMT_CONSTEXPR auto styled(const T& V, fmt::text_style ts)
 -> Styled<fmt::remove_cvref_t<T>> {
  return Styled<fmt::remove_cvref_t<T>>{V, ts};
}
#else // !EXICPP_ANSI
template <typename T>
FMT_CONSTEXPR FMT_INLINE auto styled(
 const T& V, fmt::text_style) -> const T& {
  return V;
}
#endif // EXICPP_ANSI

} // namespace dbg
} // namespace exi

#if EXICPP_ANSI
template <typename T, typename Char>
struct fmt::formatter<exi::dbg::Styled<T>, Char> :
 fmt::formatter<fmt::detail::styled_arg<T>, Char>
{
  using BaseFormatter = fmt::formatter<T, Char>;
  using AnsiFormatter = fmt::formatter<fmt::detail::styled_arg<T>, Char>;

  template <typename FormatContext>
  auto format(exi::dbg::Styled<T>& arg, FormatContext& ctx) const
   -> decltype(ctx.out()) {
    if (exi::dbg::has_ansi())
      return AnsiFormatter::format(arg, ctx);
    else
      return BaseFormatter::format(arg.value, ctx);
  }
};
#endif // EXICPP_ANSI

#endif // EXICPP_DEBUG_FORMAT_HPP
