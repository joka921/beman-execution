// include/beman/execution/detail/ext_stopped_as_error.hpp          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_STOPPED_AS_ERROR
#define INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_STOPPED_AS_ERROR

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <exception>
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.completion_signatures;
import beman.execution.detail.ext_transform_completion_channel;
import beman.execution.detail.meta.combine;
import beman.execution.detail.meta.unique;
import beman.execution.detail.movable_value;
import beman.execution.detail.sender;
import beman.execution.detail.sender_adaptor_closure;
import beman.execution.detail.set_error;
import beman.execution.detail.set_stopped;
#else
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/ext_transform_completion_channel.hpp>
#include <beman/execution/detail/meta_combine.hpp>
#include <beman/execution/detail/meta_unique.hpp>
#include <beman/execution/detail/movable_value.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_adaptor_closure.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_stopped.hpp>
#endif

// ----------------------------------------------------------------------------
// ext_stopped_as_error behaves like stopped_as_error but is implemented using
// ext_transform_completion_channel instead of being lowered to
//     let_stopped(sndr, [error]() mutable noexcept(...) { return just_error(std::move(error)); })
// The completion signatures are computed to be identical (including their
// order) to those of this lowered sender: if a stopped completion exists,
// set_error_t(Error) (from the just_error) comes first, followed by
// set_error_t(exception_ptr) if moving Error may throw, followed by the other
// completions.

namespace beman::execution::detail {
template <typename Error>
struct ext_stopped_as_error_fun {
    Error error;

    template <typename Receiver>
    auto operator()(Receiver& receiver, ::beman::execution::set_stopped_t) noexcept -> void {
        // The lowered implementation moves the error into a just_error sender
        // and its operation state before completing. Passing the stored error
        // directly avoids these moves (and the possibility of them throwing).
        ::beman::execution::set_error(::std::move(receiver), ::std::move(this->error));
    }
};

template <typename Sig>
struct ext_stopped_as_error_non_stopped {
    using type = ::beman::execution::completion_signatures<Sig>;
};
template <>
struct ext_stopped_as_error_non_stopped<::beman::execution::set_stopped_t()> {
    using type = ::beman::execution::completion_signatures<>;
};

template <typename Error>
struct ext_stopped_as_error_signatures {
    template <typename... Sigs>
    consteval auto operator()(::beman::execution::completion_signatures<Sigs...>) const {
        using others = ::beman::execution::detail::meta::combine<
            ::beman::execution::completion_signatures<>,
            typename ::beman::execution::detail::ext_stopped_as_error_non_stopped<Sigs>::type...>;
        if constexpr ((::std::same_as<Sigs, ::beman::execution::set_stopped_t()> || ...)) {
            using stopped = ::beman::execution::detail::meta::unique<::beman::execution::detail::meta::combine<
                ::beman::execution::completion_signatures<::beman::execution::set_error_t(Error)>,
                ::std::conditional_t<::std::is_nothrow_move_constructible_v<Error>,
                                     ::beman::execution::completion_signatures<>,
                                     ::beman::execution::completion_signatures<::beman::execution::set_error_t(
                                         ::std::exception_ptr)>>>>;
            return ::beman::execution::detail::meta::unique<
                ::beman::execution::detail::meta::combine<stopped, others>>{};
        } else {
            return ::beman::execution::detail::meta::unique<others>{};
        }
    }
};

/*!
 * \brief Type of the ext_stopped_as_error sender adaptor.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 *
 * Behaves like stopped_as_error but is implemented using
 * ext_transform_completion_channel, i.e., without an additional operation
 * state for the stopped completion.
 */
struct ext_stopped_as_error_t {
    template <::beman::execution::sender Sndr, ::beman::execution::detail::movable_value Error>
    auto operator()(Sndr&& sndr, Error error) const {
        return ::beman::execution::ext_transform_completion_channel(
            ::std::forward<Sndr>(sndr),
            ::beman::execution::detail::ext_stopped_as_error_fun<Error>{::std::move(error)},
            ::beman::execution::detail::ext_stopped_as_error_signatures<Error>{});
    }

    template <::beman::execution::detail::movable_value Error>
    auto operator()(Error error) const {
        return ::beman::execution::detail::make_sender_adaptor(*this, ::std::move(error));
    }
};
} // namespace beman::execution::detail

namespace beman::execution {
using ext_stopped_as_error_t = ::beman::execution::detail::ext_stopped_as_error_t;
inline constexpr ext_stopped_as_error_t ext_stopped_as_error{};
} // namespace beman::execution

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_STOPPED_AS_ERROR
