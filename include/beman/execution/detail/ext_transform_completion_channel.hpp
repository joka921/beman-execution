// include/beman/execution/detail/ext_transform_completion_channel.hpp -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_TRANSFORM_COMPLETION_CHANNEL
#define INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_TRANSFORM_COMPLETION_CHANNEL

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.basic_sender;
import beman.execution.detail.callable;
import beman.execution.detail.completion_signatures;
import beman.execution.detail.default_impls;
import beman.execution.detail.env_of_t;
import beman.execution.detail.forward_like;
import beman.execution.detail.fwd_env;
import beman.execution.detail.get_completion_signatures;
import beman.execution.detail.get_env;
import beman.execution.detail.make_sender;
import beman.execution.detail.meta.combine;
import beman.execution.detail.meta.unique;
import beman.execution.detail.movable_value;
import beman.execution.detail.nested_sender_has_affine;
import beman.execution.detail.receiver;
import beman.execution.detail.sender;
import beman.execution.detail.sender_adaptor_closure;
import beman.execution.detail.sender_in;
import beman.execution.detail.set_error;
import beman.execution.detail.set_stopped;
import beman.execution.detail.set_value;
import beman.execution.detail.valid_completion_signatures;
#else
#include <beman/execution/detail/basic_sender.hpp>
#include <beman/execution/detail/callable.hpp>
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/default_impls.hpp>
#include <beman/execution/detail/env_of_t.hpp>
#include <beman/execution/detail/forward_like.hpp>
#include <beman/execution/detail/fwd_env.hpp>
#include <beman/execution/detail/get_completion_signatures.hpp>
#include <beman/execution/detail/get_env.hpp>
#include <beman/execution/detail/make_sender.hpp>
#include <beman/execution/detail/meta_combine.hpp>
#include <beman/execution/detail/meta_unique.hpp>
#include <beman/execution/detail/movable_value.hpp>
#include <beman/execution/detail/nested_sender_has_affine.hpp>
#include <beman/execution/detail/receiver.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_adaptor_closure.hpp>
#include <beman/execution/detail/sender_in.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_stopped.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/valid_completion_signatures.hpp>
#endif

// ----------------------------------------------------------------------------
// ext_transform_completion_channel(sndr, fun, sig_transform) is a generic sender
// adaptor which can reroute the completions of sndr to (possibly different)
// completion channels:
//
// - fun is invoked as fun(rcvr, Tag(), args...) for each completion
//   Tag(args...) of sndr it is invocable with. rcvr is an lvalue receiver
//   which fun has to complete using one of set_value, set_error, or
//   set_stopped. The invocation has to be noexcept. Completions fun isn't
//   invocable with are forwarded unchanged.
// - sig_transform is a stateless, default constructible function object
//   which is called (at compile time) with the completion_signatures of
//   sndr and returns the completion_signatures of the resulting sender.
//   ext_each_completion_signature(fn) creates such a function object from a
//   function object fn transforming individual signatures.
//
// The receiver passed to fun only accepts completions declared by the result
// of sig_transform (after removing cvref qualifiers from the arguments): any
// other completion results in a compile-time error.
// ----------------------------------------------------------------------------

namespace beman::execution::detail {
template <typename SigFun>
concept completion_signatures_transform =
    ::std::is_empty_v<SigFun> && ::std::default_initializable<SigFun> && ::std::copy_constructible<SigFun>;

template <typename Fun, typename SigFun>
struct ext_transform_completion_channel_data {
    using fun_type = Fun;
    Fun                          fun;
    [[no_unique_address]] SigFun sig_transform;
};

template <typename Signature, typename Tag, typename... Args>
inline constexpr bool ext_transform_completion_channel_matches{false};
template <typename Tag, typename... S, typename... Args>
    requires(sizeof...(S) == sizeof...(Args))
inline constexpr bool ext_transform_completion_channel_matches<Tag(S...), Tag, Args...>{
    (::std::same_as<::std::remove_cvref_t<S>, ::std::remove_cvref_t<Args>> && ...)};

template <typename Signatures, typename Tag, typename... Args>
inline constexpr bool ext_transform_completion_channel_declared{false};
template <typename... Sigs, typename Tag, typename... Args>
inline constexpr bool
    ext_transform_completion_channel_declared<::beman::execution::completion_signatures<Sigs...>, Tag, Args...>{
        (::beman::execution::detail::ext_transform_completion_channel_matches<Sigs, Tag, Args...> || ...)};

/*!
 * \brief Receiver passed to the function of ext_transform_completion_channel.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 * \internal
 *
 * The receiver forwards completions to the downstream receiver but only
 * accepts completions declared by the transformed completion signatures.
 */
template <typename Receiver, typename Signatures>
struct ext_transform_completion_channel_receiver {
    using receiver_concept = ::beman::execution::receiver_tag;

    Receiver* receiver;

    template <typename... Args>
    auto set_value(Args&&... args) && noexcept -> void {
        static_assert(
            ::beman::execution::detail::
                ext_transform_completion_channel_declared<Signatures, ::beman::execution::set_value_t, Args...>,
            "ext_transform_completion_channel: set_value(args...) isn't declared by the signature transform");
        ::beman::execution::set_value(::std::move(*this->receiver), ::std::forward<Args>(args)...);
    }
    template <typename Error>
    auto set_error(Error&& error) && noexcept -> void {
        static_assert(
            ::beman::execution::detail::
                ext_transform_completion_channel_declared<Signatures, ::beman::execution::set_error_t, Error>,
            "ext_transform_completion_channel: set_error(error) isn't declared by the signature transform");
        ::beman::execution::set_error(::std::move(*this->receiver), ::std::forward<Error>(error));
    }
    auto set_stopped() && noexcept -> void {
        static_assert(
            ::beman::execution::detail::ext_transform_completion_channel_declared<Signatures,
                                                                                  ::beman::execution::set_stopped_t>,
            "ext_transform_completion_channel: set_stopped() isn't declared by the signature transform");
        ::beman::execution::set_stopped(::std::move(*this->receiver));
    }
    auto get_env() const noexcept -> decltype(auto) { return ::beman::execution::get_env(*this->receiver); }
};

template <typename Fun, typename Signatures>
struct ext_transform_completion_channel_state {
    using signatures = Signatures;
    Fun fun;
};

/*!
 * \brief Function object transforming each completion signature individually.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 *
 * The function object Fn is called with a null pointer of type Sig* for each
 * signature Sig and has to return a completion_signatures object. If Fn
 * isn't invocable with a Sig* the signature is retained unchanged. The
 * results are combined, removing duplicates.
 */
template <typename Fn>
    requires ::beman::execution::detail::completion_signatures_transform<Fn>
struct ext_each_completion_signature_t {
  private:
    template <typename Sig>
    static consteval auto apply() {
        if constexpr (requires { Fn{}(static_cast<Sig*>(nullptr)); }) {
            using type = decltype(Fn{}(static_cast<Sig*>(nullptr)));
            static_assert(::beman::execution::detail::valid_completion_signatures<type>,
                          "ext_each_completion_signature: the transform has to return completion_signatures");
            return Fn{}(static_cast<Sig*>(nullptr));
        } else {
            return ::beman::execution::completion_signatures<Sig>{};
        }
    }

  public:
    template <typename... Sigs>
    consteval auto operator()(::beman::execution::completion_signatures<Sigs...>) const {
        return ::beman::execution::detail::meta::unique<
            ::beman::execution::detail::meta::combine<::beman::execution::completion_signatures<>,
                                                      decltype(ext_each_completion_signature_t::apply<Sigs>())...>>{};
    }
};

struct ext_each_completion_signature_fn {
    template <::beman::execution::detail::completion_signatures_transform Fn>
    constexpr auto operator()(Fn) const noexcept -> ::beman::execution::detail::ext_each_completion_signature_t<Fn> {
        return {};
    }
};

/*!
 * \brief Type of the ext_transform_completion_channel sender adaptor.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 */
struct ext_transform_completion_channel_t {
    template <::beman::execution::detail::movable_value                   Fun,
              ::beman::execution::detail::completion_signatures_transform SigFun>
    auto operator()(Fun&& fun, SigFun sig_transform) const
        noexcept(::std::is_nothrow_constructible_v<::std::remove_cvref_t<Fun>, Fun>) {
        return ::beman::execution::detail::make_sender_adaptor(*this, ::std::forward<Fun>(fun), sig_transform);
    }
    template <::beman::execution::sender                                  Sender,
              ::beman::execution::detail::movable_value                   Fun,
              ::beman::execution::detail::completion_signatures_transform SigFun>
    auto operator()(Sender&& sender, Fun&& fun, SigFun sig_transform) const
        noexcept(::std::is_nothrow_constructible_v<::std::remove_cvref_t<Sender>, Sender> &&
                 ::std::is_nothrow_constructible_v<::std::remove_cvref_t<Fun>, Fun>) {
        return ::beman::execution::detail::make_sender(
            *this,
            ::beman::execution::detail::ext_transform_completion_channel_data<::std::remove_cvref_t<Fun>, SigFun>{
                ::std::forward<Fun>(fun), sig_transform},
            ::std::forward<Sender>(sender));
    }
    template <::beman::execution::sender Sender>
        requires ::beman::execution::detail::nested_sender_has_affine<Sender>
    static auto affine(Sender&& sndr) noexcept {
        return ::std::forward<Sender>(sndr);
    }

  private:
    template <typename, typename...>
    struct get_signatures;

    template <typename Fun, typename SigFun, typename Child, typename... Env>
        requires ::beman::execution::sender_in<Child, ::beman::execution::detail::fwd_env_t<Env>...>
    struct get_signatures<::beman::execution::detail::basic_sender<
                              ::beman::execution::detail::ext_transform_completion_channel_t,
                              ::beman::execution::detail::ext_transform_completion_channel_data<Fun, SigFun>,
                              Child>,
                          Env...> {
        static consteval auto get() {
            using child_sigs_t =
                decltype(::beman::execution::
                             get_completion_signatures<Child, ::beman::execution::detail::fwd_env_t<Env>...>());
            using type = decltype(SigFun{}(child_sigs_t{}));
            static_assert(
                ::beman::execution::detail::valid_completion_signatures<type>,
                "ext_transform_completion_channel: the signature transform has to return completion_signatures");
            return SigFun{}(child_sigs_t{});
        }
    };

  public:
    template <typename Sender, typename... Env>
        requires requires { get_signatures<::std::remove_cvref_t<Sender>, Env...>{}; }
    static consteval auto get_completion_signatures() {
        return get_signatures<::std::remove_cvref_t<Sender>, Env...>::get();
    }

    struct impls_for : ::beman::execution::detail::default_impls {
        struct get_state_impl {
            template <typename Sender, typename Receiver>
            auto operator()(Sender&& sender, Receiver&) const
                noexcept(::std::is_nothrow_move_constructible_v<
                         typename ::std::remove_cvref_t<decltype(sender.template get<1>())>::fun_type>) {
                auto&& data = sender.template get<1>();
                using fun_t = typename ::std::remove_cvref_t<decltype(data)>::fun_type;
                using sigs_t =
                    decltype(ext_transform_completion_channel_t::
                                 get_completion_signatures<Sender, ::beman::execution::env_of_t<Receiver>>());
                return ::beman::execution::detail::ext_transform_completion_channel_state<fun_t, sigs_t>{
                    ::beman::execution::detail::forward_like<Sender>(data.fun)};
            }
        };
        static constexpr auto get_state{get_state_impl{}};

        struct complete_impl {
            template <typename Index, typename State, typename Receiver, typename Tag, typename... Args>
                requires ::std::invocable<decltype(::std::declval<State&>().fun),
                                          ::beman::execution::detail::ext_transform_completion_channel_receiver<
                                              Receiver,
                                              typename State::signatures>&,
                                          Tag,
                                          Args...> ||
                         ::beman::execution::detail::callable<Tag, Receiver, Args...>
            auto operator()(Index, State& state, Receiver& receiver, Tag, Args&&... args) const noexcept -> void {
                using fun_t = decltype(::std::declval<State&>().fun);
                using receiver_t =
                    ::beman::execution::detail::ext_transform_completion_channel_receiver<Receiver,
                                                                                          typename State::signatures>;
                if constexpr (::std::invocable<fun_t, receiver_t&, Tag, Args...>) {
                    static_assert(::std::is_nothrow_invocable_v<fun_t, receiver_t&, Tag, Args...>,
                                  "ext_transform_completion_channel: the function has to be noexcept");
                    receiver_t rcvr{&receiver};
                    ::std::invoke(::std::move(state.fun), rcvr, Tag(), ::std::forward<Args>(args)...);
                } else {
                    Tag()(::std::move(receiver), ::std::forward<Args>(args)...);
                }
            }
        };
        static constexpr auto complete{complete_impl{}};
    };
};
} // namespace beman::execution::detail

namespace beman::execution {
using ext_transform_completion_channel_t = ::beman::execution::detail::ext_transform_completion_channel_t;
inline constexpr ext_transform_completion_channel_t ext_transform_completion_channel{};

template <typename Fn>
using ext_each_completion_signature_t = ::beman::execution::detail::ext_each_completion_signature_t<Fn>;
inline constexpr ::beman::execution::detail::ext_each_completion_signature_fn ext_each_completion_signature{};
} // namespace beman::execution

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_TRANSFORM_COMPLETION_CHANNEL
