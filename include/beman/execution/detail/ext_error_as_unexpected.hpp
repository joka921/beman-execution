// include/beman/execution/detail/ext_error_as_unexpected.hpp         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_ERROR_AS_UNEXPECTED
#define INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_ERROR_AS_UNEXPECTED

#include <version>
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <exception>
#if defined(__cpp_lib_expected)
#include <expected>
#endif
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.completion_signatures;
import beman.execution.detail.ext_transform_completion_channel;
import beman.execution.detail.meta.combine;
import beman.execution.detail.meta.unique;
import beman.execution.detail.sender;
import beman.execution.detail.sender_adaptor_closure;
import beman.execution.detail.set_error;
import beman.execution.detail.set_stopped;
import beman.execution.detail.set_value;
#else
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/ext_transform_completion_channel.hpp>
#include <beman/execution/detail/meta_combine.hpp>
#include <beman/execution/detail/meta_unique.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_adaptor_closure.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_stopped.hpp>
#include <beman/execution/detail/set_value.hpp>
#endif

// ----------------------------------------------------------------------------
// ext_error_as_unexpected(sndr) is the counterpart of ext_unexpected_as_error:
// sndr has to have exactly one value completion set_value_t(V) (or
// set_value_t()) and exactly one error completion set_error_t(E). The
// resulting sender completes with set_value(std::expected<decay_t<V>,
// decay_t<E>>) (std::expected<void, decay_t<E>> for set_value_t()) for both
// of these completions. A stopped completion is forwarded unchanged.
// Similar to stopped_as_optional, set_error_t(exception_ptr) is added to the
// completion signatures if constructing the expected may throw.

#if defined(__cpp_lib_expected)

namespace beman::execution::detail {
template <typename Sig>
struct ext_error_as_unexpected_value {
    static constexpr unsigned count{0u};
    using type = ::beman::execution::completion_signatures<>;
};
template <typename... Args>
struct ext_error_as_unexpected_value<::beman::execution::set_value_t(Args...)> {
    static constexpr unsigned count{1u};
    using type = ::beman::execution::completion_signatures<::beman::execution::set_value_t(Args...)>;
};
template <typename Sig>
struct ext_error_as_unexpected_error {
    static constexpr unsigned count{0u};
    using type = ::beman::execution::completion_signatures<>;
};
template <typename Error>
struct ext_error_as_unexpected_error<::beman::execution::set_error_t(Error)> {
    static constexpr unsigned count{1u};
    using type = ::beman::execution::completion_signatures<::beman::execution::set_error_t(Error)>;
};

template <typename ValueSig, typename ErrorSig>
struct ext_error_as_unexpected_types;
template <typename... Args, typename Error>
struct ext_error_as_unexpected_types<
    ::beman::execution::completion_signatures<::beman::execution::set_value_t(Args...)>,
    ::beman::execution::completion_signatures<::beman::execution::set_error_t(Error)>> {
    static_assert(sizeof...(Args) <= 1, "ext_error_as_unexpected: the value completion can have at most one argument");
};
template <typename Arg, typename Error>
struct ext_error_as_unexpected_types<
    ::beman::execution::completion_signatures<::beman::execution::set_value_t(Arg)>,
    ::beman::execution::completion_signatures<::beman::execution::set_error_t(Error)>> {
    using expected_type = ::std::expected<::std::remove_cvref_t<Arg>, ::std::remove_cvref_t<Error>>;
    static constexpr bool nothrow{::std::is_nothrow_constructible_v<expected_type, ::std::in_place_t, Arg> &&
                                  ::std::is_nothrow_constructible_v<expected_type, ::std::unexpect_t, Error>};
};
template <typename Error>
struct ext_error_as_unexpected_types<
    ::beman::execution::completion_signatures<::beman::execution::set_value_t()>,
    ::beman::execution::completion_signatures<::beman::execution::set_error_t(Error)>> {
    using expected_type = ::std::expected<void, ::std::remove_cvref_t<Error>>;
    static constexpr bool nothrow{::std::is_nothrow_constructible_v<expected_type, ::std::unexpect_t, Error>};
};

struct ext_error_as_unexpected_signatures {
    template <typename... Sigs>
    consteval auto operator()(::beman::execution::completion_signatures<Sigs...>) const {
        using values = ::beman::execution::detail::meta::combine<
            ::beman::execution::completion_signatures<>,
            typename ::beman::execution::detail::ext_error_as_unexpected_value<Sigs>::type...>;
        using errors = ::beman::execution::detail::meta::combine<
            ::beman::execution::completion_signatures<>,
            typename ::beman::execution::detail::ext_error_as_unexpected_error<Sigs>::type...>;
        static_assert((0u + ... + ::beman::execution::detail::ext_error_as_unexpected_value<Sigs>::count) == 1u,
                      "ext_error_as_unexpected: the sender needs exactly one value completion");
        static_assert((0u + ... + ::beman::execution::detail::ext_error_as_unexpected_error<Sigs>::count) == 1u,
                      "ext_error_as_unexpected: the sender needs exactly one error completion");
        using types = ::beman::execution::detail::ext_error_as_unexpected_types<values, errors>;

        return ::beman::execution::detail::meta::unique<::beman::execution::detail::meta::combine<
            ::beman::execution::completion_signatures<::beman::execution::set_value_t(typename types::expected_type)>,
            ::std::conditional_t<
                types::nothrow,
                ::beman::execution::completion_signatures<>,
                ::beman::execution::completion_signatures<::beman::execution::set_error_t(::std::exception_ptr)>>,
            ::std::conditional_t<(::std::same_as<Sigs, ::beman::execution::set_stopped_t()> || ...),
                                 ::beman::execution::completion_signatures<::beman::execution::set_stopped_t()>,
                                 ::beman::execution::completion_signatures<>>>>{};
    }
};

// The transformed signatures contain exactly one value completion:
// set_value_t(std::expected<V, E>). The signatures passed to the function are
// used to determine this type as each completion of the sender only knows V or E.
struct ext_error_as_unexpected_fun : ::beman::execution::detail::ext_forward_completion {
    using ::beman::execution::detail::ext_forward_completion::operator();
    template <typename Signatures, typename Receiver, typename Construct>
    static auto complete(Receiver& receiver, Construct construct) noexcept -> void {
        using expected_t = ::beman::execution::detail::ext_single_value_argument_t<Signatures>;
        if constexpr (noexcept(construct.template operator()<expected_t>())) {
            ::beman::execution::set_value(::std::move(receiver), construct.template operator()<expected_t>());
        } else {
            try {
                ::beman::execution::set_value(::std::move(receiver), construct.template operator()<expected_t>());
            } catch (...) {
                ::beman::execution::set_error(::std::move(receiver), ::std::current_exception());
            }
        }
    }

    template <typename Receiver, typename Signatures, typename... Args>
        requires(sizeof...(Args) <= 1)
    auto operator()(Receiver&& receiver, Signatures, ::beman::execution::set_value_t, Args&&... args) const noexcept
        -> void {
        complete<Signatures>(
            receiver,
            [&]<typename Exp>() noexcept(::std::is_nothrow_constructible_v<Exp, ::std::in_place_t, Args...>) {
                return Exp(::std::in_place, ::std::forward<Args>(args)...);
            });
    }
    template <typename Receiver, typename Signatures, typename Error>
    auto operator()(Receiver&& receiver, Signatures, ::beman::execution::set_error_t, Error&& error) const noexcept
        -> void {
        complete<Signatures>(
            receiver, [&]<typename Exp>() noexcept(::std::is_nothrow_constructible_v<Exp, ::std::unexpect_t, Error>) {
                return Exp(::std::unexpect_t{}, ::std::forward<Error>(error));
            });
    }
};

/*!
 * \brief Type of the ext_error_as_unexpected sender adaptor.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 *
 * Turns the single value completion set_value(v) and the single error
 * completion set_error(e) of a sender into set_value(std::expected<V, E>)
 * holding v or the error e, respectively. A stopped completion is forwarded
 * unchanged.
 */
struct ext_error_as_unexpected_t {
    template <::beman::execution::sender Sndr>
    auto operator()(Sndr&& sndr) const {
        return ::beman::execution::ext_transform_completion_channel(
            ::std::forward<Sndr>(sndr),
            ::beman::execution::detail::ext_error_as_unexpected_fun{},
            ::beman::execution::detail::ext_error_as_unexpected_signatures{});
    }

    auto operator()() const noexcept { return ::beman::execution::detail::make_sender_adaptor(*this); }
};
} // namespace beman::execution::detail

namespace beman::execution {
using ext_error_as_unexpected_t = ::beman::execution::detail::ext_error_as_unexpected_t;
inline constexpr ext_error_as_unexpected_t ext_error_as_unexpected{};
} // namespace beman::execution

#endif // defined(__cpp_lib_expected)

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_ERROR_AS_UNEXPECTED
