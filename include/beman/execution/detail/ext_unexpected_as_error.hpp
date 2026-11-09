// include/beman/execution/detail/ext_unexpected_as_error.hpp         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_UNEXPECTED_AS_ERROR
#define INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_UNEXPECTED_AS_ERROR

#include <version>
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#if defined(__cpp_lib_expected)
#include <expected>
#endif
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.completion_signatures;
import beman.execution.detail.sender;
import beman.execution.detail.sender_adaptor_closure;
import beman.execution.detail.set_error;
import beman.execution.detail.set_value;
import beman.execution.detail.ext_transform_completion_channel;
#else
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_adaptor_closure.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/ext_transform_completion_channel.hpp>
#endif

// ----------------------------------------------------------------------------

#if defined(__cpp_lib_expected)

namespace beman::execution::detail {
template <typename>
struct is_expected_helper : ::std::false_type {};
template <typename T, typename E>
struct is_expected_helper<::std::expected<T, E>> : ::std::true_type {};

template <typename T>
concept is_expected = ::beman::execution::detail::is_expected_helper<::std::remove_cvref_t<T>>::value;

struct ext_unexpected_as_error_fun : ::beman::execution::detail::ext_forward_completion {
    using ::beman::execution::detail::ext_forward_completion::operator();
    template <typename Receiver, typename Signatures, ::beman::execution::detail::is_expected Exp>
    auto operator()(Receiver&& receiver, Signatures, ::beman::execution::set_value_t, Exp&& exp) const noexcept
        -> void {
        if (exp) {
            if constexpr (::std::is_void_v<typename ::std::remove_cvref_t<Exp>::value_type>) {
                ::beman::execution::set_value(::std::move(receiver));
            } else {
                ::beman::execution::set_value(::std::move(receiver), *::std::forward<Exp>(exp));
            }
        } else {
            ::beman::execution::set_error(::std::move(receiver), ::std::forward<Exp>(exp).error());
        }
    }
};

struct ext_unexpected_as_error_signatures {
    template <::beman::execution::detail::is_expected Exp>
    consteval auto operator()(::beman::execution::set_value_t (*)(Exp)) const noexcept {
        using value_type = typename ::std::remove_cvref_t<Exp>::value_type;
        using error_type = typename ::std::remove_cvref_t<Exp>::error_type;
        if constexpr (::std::is_void_v<value_type>) {
            return ::beman::execution::completion_signatures<::beman::execution::set_value_t(),
                                                             ::beman::execution::set_error_t(error_type)>{};
        } else {
            return ::beman::execution::completion_signatures<::beman::execution::set_value_t(value_type),
                                                             ::beman::execution::set_error_t(error_type)>{};
        }
    }
};

/*!
 * \brief Type of the ext_unexpected_as_error sender adaptor.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 *
 * Value completions with a single std::expected<T, E> argument complete with
 * set_value(T) (or set_value() if T is void) if the expected holds a value
 * and with set_error(E) otherwise. All other completions are forwarded
 * unchanged.
 */
struct ext_unexpected_as_error_t {
    template <::beman::execution::sender Sndr>
    auto operator()(Sndr&& sndr) const {
        return ::beman::execution::ext_transform_completion_channel(
            ::std::forward<Sndr>(sndr),
            ::beman::execution::detail::ext_unexpected_as_error_fun{},
            ::beman::execution::ext_each_completion_signature(
                ::beman::execution::detail::ext_unexpected_as_error_signatures{}));
    }

    auto operator()() const noexcept { return ::beman::execution::detail::make_sender_adaptor(*this); }
};
} // namespace beman::execution::detail

namespace beman::execution {
using ext_unexpected_as_error_t = ::beman::execution::detail::ext_unexpected_as_error_t;
inline constexpr ext_unexpected_as_error_t ext_unexpected_as_error{};
} // namespace beman::execution

#endif // defined(__cpp_lib_expected)

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_UNEXPECTED_AS_ERROR
