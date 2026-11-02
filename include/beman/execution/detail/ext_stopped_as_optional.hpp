// include/beman/execution/detail/ext_stopped_as_optional.hpp       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_STOPPED_AS_OPTIONAL
#define INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_STOPPED_AS_OPTIONAL

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.completion_signatures;
import beman.execution.detail.env;
import beman.execution.detail.ext_transform_completion_channel;
import beman.execution.detail.meta.combine;
import beman.execution.detail.meta.unique;
import beman.execution.detail.sender;
import beman.execution.detail.sender_adaptor_closure;
import beman.execution.detail.set_error;
import beman.execution.detail.set_stopped;
import beman.execution.detail.set_value;
import beman.execution.detail.single_sender;
import beman.execution.detail.single_sender_value_type;
#else
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/env.hpp>
#include <beman/execution/detail/ext_transform_completion_channel.hpp>
#include <beman/execution/detail/meta_combine.hpp>
#include <beman/execution/detail/meta_unique.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_adaptor_closure.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_stopped.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/single_sender.hpp>
#include <beman/execution/detail/single_sender_value_type.hpp>
#endif

// ----------------------------------------------------------------------------
// ext_stopped_as_optional behaves like stopped_as_optional but is implemented
// using ext_transform_completion_channel instead of being lowered to
//     let_stopped(then(sndr, make-optional), []{ return just(optional<V>()); })
// The completion signatures are computed to be identical (including their
// order) to those of this lowered sender:
// - then: each value completion becomes set_value_t(optional<V>) and
//   set_error_t(exception_ptr) is appended if constructing V may throw.
// - let_stopped: if a stopped completion exists, set_value_t(optional<V>)
//   (from the just) comes first, followed by the other completions.

namespace beman::execution::detail {
/*!
 * \brief Sender with the given completion signatures, used to reuse the sender-based value type helpers.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 * \internal
 */
template <typename Signatures>
struct ext_signatures_sender {
    using sender_concept = ::beman::execution::sender_tag;
    template <typename, typename...>
    static consteval auto get_completion_signatures() -> Signatures {
        return {};
    }
};

template <typename Opt, typename Sig>
struct ext_stopped_as_optional_then_sig {
    using type = ::beman::execution::completion_signatures<Sig>;
};
template <typename Opt, typename... Args>
struct ext_stopped_as_optional_then_sig<Opt, ::beman::execution::set_value_t(Args...)> {
    using type = ::beman::execution::completion_signatures<::beman::execution::set_value_t(Opt)>;
};

template <typename Value, typename Sig>
inline constexpr bool ext_stopped_as_optional_may_throw{false};
template <typename Value, typename... Args>
inline constexpr bool ext_stopped_as_optional_may_throw<Value, ::beman::execution::set_value_t(Args...)>{
    not ::std::is_nothrow_constructible_v<Value, Args...>};

template <typename Sig>
struct ext_stopped_as_optional_non_stopped {
    using type = ::beman::execution::completion_signatures<Sig>;
};
template <>
struct ext_stopped_as_optional_non_stopped<::beman::execution::set_stopped_t()> {
    using type = ::beman::execution::completion_signatures<>;
};

struct ext_stopped_as_optional_signatures {
    template <typename... Sigs>
    consteval auto operator()(::beman::execution::completion_signatures<Sigs...>) const {
        using sender_t =
            ::beman::execution::detail::ext_signatures_sender<::beman::execution::completion_signatures<Sigs...>>;
        static_assert(::beman::execution::detail::single_sender<sender_t, ::beman::execution::env<>>,
                      "sender must be a single sender");
        using value_type = ::beman::execution::detail::single_sender_value_type<sender_t, ::beman::execution::env<>>;
        static_assert(!::std::is_void_v<value_type>, "the value type of sender can't be empty");
        using opt_t = ::std::optional<value_type>;

        // completions of then(sndr, make-optional)
        using then_sigs = ::beman::execution::detail::meta::unique<::beman::execution::detail::meta::combine<
            ::beman::execution::completion_signatures<>,
            typename ::beman::execution::detail::ext_stopped_as_optional_then_sig<opt_t, Sigs>::type...,
            ::std::conditional_t<
                (::beman::execution::detail::ext_stopped_as_optional_may_throw<value_type, Sigs> || ...),
                ::beman::execution::completion_signatures<::beman::execution::set_error_t(::std::exception_ptr)>,
                ::beman::execution::completion_signatures<>>>>;
        // completions of let_stopped(then-sender, []() noexcept { return just(opt_t()); })
        if constexpr ((::std::same_as<Sigs, ::beman::execution::set_stopped_t()> || ...)) {
            return ::beman::execution::detail::meta::unique<::beman::execution::detail::meta::combine<
                ::beman::execution::completion_signatures<::beman::execution::set_value_t(opt_t)>,
                filter_stopped<then_sigs>>>{};
        } else {
            return then_sigs{};
        }
    }

  private:
    template <typename>
    struct filter_stopped_helper;
    template <typename... S>
    struct filter_stopped_helper<::beman::execution::completion_signatures<S...>> {
        using type = ::beman::execution::detail::meta::combine<
            ::beman::execution::completion_signatures<>,
            typename ::beman::execution::detail::ext_stopped_as_optional_non_stopped<S>::type...>;
    };
    template <typename Sigs>
    using filter_stopped = typename filter_stopped_helper<Sigs>::type;
};

// the transformed signatures contain exactly one value completion: set_value_t(optional<V>)
struct ext_stopped_as_optional_fun {
    template <typename Receiver, typename... Args>
    auto operator()(Receiver& receiver, ::beman::execution::set_value_t, Args&&... args) const noexcept -> void {
        using opt_t =
            ::beman::execution::detail::ext_single_value_argument_t<typename Receiver::completion_signatures>;
        using value_type = typename opt_t::value_type;
        try {
            ::beman::execution::set_value(::std::move(receiver),
                                          opt_t(::std::in_place, ::std::forward<Args>(args)...));
        } catch (...) {
            if constexpr (not ::std::is_nothrow_constructible_v<value_type, Args...>) {
                ::beman::execution::set_error(::std::move(receiver), ::std::current_exception());
            }
        }
    }
    template <typename Receiver>
    auto operator()(Receiver& receiver, ::beman::execution::set_stopped_t) const noexcept -> void {
        using opt_t =
            ::beman::execution::detail::ext_single_value_argument_t<typename Receiver::completion_signatures>;
        ::beman::execution::set_value(::std::move(receiver), opt_t());
    }
};

/*!
 * \brief Type of the ext_stopped_as_optional sender adaptor.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 *
 * Behaves like stopped_as_optional but is implemented using
 * ext_transform_completion_channel, i.e., without an additional operation
 * state for the stopped completion.
 */
struct ext_stopped_as_optional_t {
    template <::beman::execution::sender Sndr>
    auto operator()(Sndr&& sndr) const {
        return ::beman::execution::ext_transform_completion_channel(
            ::std::forward<Sndr>(sndr),
            ::beman::execution::detail::ext_stopped_as_optional_fun{},
            ::beman::execution::detail::ext_stopped_as_optional_signatures{});
    }

    auto operator()() const noexcept { return ::beman::execution::detail::make_sender_adaptor(*this); }
};
} // namespace beman::execution::detail

namespace beman::execution {
using ext_stopped_as_optional_t = ::beman::execution::detail::ext_stopped_as_optional_t;
inline constexpr ext_stopped_as_optional_t ext_stopped_as_optional{};
} // namespace beman::execution

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_STOPPED_AS_OPTIONAL
