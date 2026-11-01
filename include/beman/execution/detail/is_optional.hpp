// include/beman/execution/detail/is_optional.hpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_IS_OPTIONAL
#define INCLUDED_BEMAN_EXECUTION_DETAIL_IS_OPTIONAL

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <optional>
#include <type_traits>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {
template <typename>
struct is_optional_helper : ::std::false_type {};
template <typename T>
struct is_optional_helper<::std::optional<T>> : ::std::true_type {};

/*!
 * \brief Concept determining whether a type is a (possibly cvref qualified) std::optional<T>.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 * \internal
 */
template <typename T>
concept is_optional = ::beman::execution::detail::is_optional_helper<::std::remove_cvref_t<T>>::value;
} // namespace beman::execution::detail

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_IS_OPTIONAL
