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
#include <memory>
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.allocator_aware_move;
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
import beman.execution.detail.type_list;
import beman.execution.detail.valid_completion_signatures;
#else
#include <beman/execution/detail/allocator_aware_move.hpp>
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
#include <beman/execution/detail/type_list.hpp>
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
//
// If sndr is itself an ext_transform_completion_channel sender, the two
// senders are merged into one sender whose function object holds both
// functions. The completions are dispatched as by the nested senders but a
// chain of these adaptors uses only one operation state, and connecting it
// doesn't move the child sender through each of the nested operation states.
// ----------------------------------------------------------------------------

namespace beman::execution::detail {
template <typename SigFun>
concept completion_signatures_transform =
    ::std::is_empty_v<SigFun> && ::std::default_initializable<SigFun> && ::std::copy_constructible<SigFun>;

template <typename Fun, typename SigFun>
struct ext_transform_completion_channel_data {
    using fun_type = Fun;
    using sig_type = SigFun;
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
 * These signatures are available as the member type completion_signatures,
 * e.g., to determine the types of completions which can't be derived from
 * the arguments of the transformed completion.
 */
template <typename>
inline constexpr bool ext_transform_completion_channel_by_value{false};

template <typename Receiver, typename Signatures>
struct ext_transform_completion_channel_receiver {
    using receiver_concept      = ::beman::execution::receiver_tag;
    using completion_signatures = Signatures;

    // the receivers used inside merged senders are cheap to copy and stored by value
    ::std::conditional_t<::beman::execution::detail::ext_transform_completion_channel_by_value<Receiver>,
                         Receiver,
                         Receiver*>
        receiver;

    auto target() noexcept -> Receiver& {
        if constexpr (::beman::execution::detail::ext_transform_completion_channel_by_value<Receiver>)
            return this->receiver;
        else
            return *this->receiver;
    }
    auto target() const noexcept -> const Receiver& {
        if constexpr (::beman::execution::detail::ext_transform_completion_channel_by_value<Receiver>)
            return this->receiver;
        else
            return *this->receiver;
    }

    template <typename... Args>
    auto set_value(Args&&... args) && noexcept -> void {
        static_assert(
            ::beman::execution::detail::
                ext_transform_completion_channel_declared<Signatures, ::beman::execution::set_value_t, Args...>,
            "ext_transform_completion_channel: set_value(args...) isn't declared by the signature transform");
        ::beman::execution::set_value(::std::move(this->target()), ::std::forward<Args>(args)...);
    }
    template <typename Error>
    auto set_error(Error&& error) && noexcept -> void {
        static_assert(
            ::beman::execution::detail::
                ext_transform_completion_channel_declared<Signatures, ::beman::execution::set_error_t, Error>,
            "ext_transform_completion_channel: set_error(error) isn't declared by the signature transform");
        ::beman::execution::set_error(::std::move(this->target()), ::std::forward<Error>(error));
    }
    auto set_stopped() && noexcept -> void {
        static_assert(
            ::beman::execution::detail::ext_transform_completion_channel_declared<Signatures,
                                                                                  ::beman::execution::set_stopped_t>,
            "ext_transform_completion_channel: set_stopped() isn't declared by the signature transform");
        ::beman::execution::set_stopped(::std::move(this->target()));
    }
    auto get_env() const noexcept -> decltype(auto) { return ::beman::execution::get_env(this->target()); }
};

// Paths selecting the function of a (possibly composed) function object.
struct ext_transform_completion_channel_path_root {
    template <typename Root>
    static auto get(Root& root) noexcept -> Root& {
        return root;
    }
};
template <typename Path>
struct ext_transform_completion_channel_path_inner {
    template <typename Root>
    static auto get(Root& root) noexcept -> auto& {
        return Path::get(root).inner;
    }
};
template <typename Path>
struct ext_transform_completion_channel_path_outer {
    template <typename Root>
    static auto get(Root& root) noexcept -> auto& {
        return Path::get(root).outer;
    }
};

// The last continuation: deliver the completion to the downstream receiver.
struct ext_transform_completion_channel_deliver {
    template <typename Root, typename Receiver, typename Tag, typename... Args>
    static constexpr bool can_complete{::beman::execution::detail::callable<Tag, Receiver, Args...>};

    template <typename Root, typename Receiver, typename Tag, typename... Args>
    static auto complete(Root&, Receiver& receiver, Tag, Args&&... args) noexcept -> void {
        Tag()(::std::move(receiver), ::std::forward<Args>(args)...);
    }
};

/*!
 * \brief Receiver continuing with the next function of a merged sender.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 * \internal
 *
 * It plays the role of the receiver of a nested operation state: the
 * completions are passed to the continuation Cont. All of these receivers
 * consist of the same two pointers (the function object of the merged sender
 * and the downstream receiver) independent of the position in the chain.
 */
template <typename Root, typename Receiver, typename Cont>
struct ext_transform_completion_channel_chain_receiver {
    using receiver_concept = ::beman::execution::receiver_tag;
    Root*     root;
    Receiver* rcvr;

    template <typename... Args>
        requires Cont::template
    can_complete<Root, Receiver, ::beman::execution::set_value_t, Args...> auto set_value(Args&&... args) && noexcept
        -> void {
        Cont::complete(*this->root, *this->rcvr, ::beman::execution::set_value_t(), ::std::forward<Args>(args)...);
    }
    template <typename Error>
        requires Cont::template
    can_complete<Root, Receiver, ::beman::execution::set_error_t, Error> auto set_error(Error&& error) && noexcept
        -> void {
        Cont::complete(*this->root, *this->rcvr, ::beman::execution::set_error_t(), ::std::forward<Error>(error));
    }
    auto set_stopped() && noexcept -> void
        requires Cont::template
    can_complete<Root, Receiver, ::beman::execution::set_stopped_t> {
        Cont::complete(*this->root, *this->rcvr, ::beman::execution::set_stopped_t());
    }
    // like the environment of the receiver of a nested operation state
    auto get_env() const noexcept -> decltype(auto) {
        return ::beman::execution::detail::fwd_env(::beman::execution::get_env(*this->rcvr));
    }
};
template <typename Root, typename Receiver, typename Cont>
inline constexpr bool ext_transform_completion_channel_by_value<
    ::beman::execution::detail::ext_transform_completion_channel_chain_receiver<Root, Receiver, Cont>>{true};

/*!
 * \brief Completes a receiver with a completion transformed by a function.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 * \internal
 *
 * ChildSigs are the completion signatures of the adapted sender and SigFun
 * the signature transform. The function is Path::get(root). If it is
 * invocable with the completion it is called with a checking receiver;
 * otherwise the completion is forwarded. Either way, the resulting
 * completion is passed on to the continuation Cont.
 */
template <typename Fun,
          typename SigFun,
          typename ChildSigs,
          typename Path = ::beman::execution::detail::ext_transform_completion_channel_path_root,
          typename Cont = ::beman::execution::detail::ext_transform_completion_channel_deliver>
struct ext_transform_completion_channel_dispatch {
    using signatures = decltype(SigFun{}(ChildSigs{}));
    static_assert(::beman::execution::detail::valid_completion_signatures<signatures>,
                  "ext_transform_completion_channel: the signature transform has to return completion_signatures");

    static constexpr bool deliver{
        ::std::same_as<Cont, ::beman::execution::detail::ext_transform_completion_channel_deliver>};
    template <typename Root, typename Receiver>
    using target_t = ::std::conditional_t<
        deliver,
        Receiver,
        ::beman::execution::detail::ext_transform_completion_channel_chain_receiver<Root, Receiver, Cont>>;
    template <typename Root, typename Receiver>
    using receiver_t =
        ::beman::execution::detail::ext_transform_completion_channel_receiver<target_t<Root, Receiver>, signatures>;

    template <typename Root, typename Receiver, typename Tag, typename... Args>
    static constexpr bool can_complete{::std::invocable<Fun, receiver_t<Root, Receiver>&, Tag, Args...> ||
                                       Cont::template can_complete<Root, Receiver, Tag, Args...>};

    template <typename Root, typename Receiver, typename Tag, typename... Args>
    static auto complete(Root& root, Receiver& receiver, Tag, Args&&... args) noexcept -> void {
        if constexpr (::std::invocable<Fun, receiver_t<Root, Receiver>&, Tag, Args...>) {
            static_assert(::std::is_nothrow_invocable_v<Fun, receiver_t<Root, Receiver>&, Tag, Args...>,
                          "ext_transform_completion_channel: the function has to be noexcept");
            receiver_t<Root, Receiver> rcvr{[&] {
                if constexpr (deliver)
                    return &receiver;
                else
                    return target_t<Root, Receiver>{&root, &receiver};
            }()};
            ::std::invoke(::std::move(Path::get(root)), rcvr, Tag(), ::std::forward<Args>(args)...);
        } else {
            Cont::complete(root, receiver, Tag(), ::std::forward<Args>(args)...);
        }
    }
};

/*!
 * \brief The functions of two nested ext_transform_completion_channel senders.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 * \internal
 *
 * ext_transform_completion_channel(ext_transform_completion_channel(sndr, f1, s1), f2, s2)
 * is merged into a single sender ext_transform_completion_channel(sndr,
 * composed{f1, f2}, composed_signatures<s1, s2>) such that a chain of these
 * adaptors uses just one operation state. The completions are dispatched
 * exactly as by the nested senders.
 */
template <typename Inner, typename Outer>
struct ext_transform_completion_channel_composed {
    [[no_unique_address]] Inner inner;
    [[no_unique_address]] Outer outer;

    template <typename I, typename O>
    ext_transform_completion_channel_composed(I&& i, O&& o) noexcept(::std::is_nothrow_constructible_v<Inner, I> &&
                                                                     ::std::is_nothrow_constructible_v<Outer, O>)
        : inner(::std::forward<I>(i)), outer(::std::forward<O>(o)) {}
    // Allocator-extended constructors: like for nested senders each function
    // is constructed using the receiver's allocator if it is allocator-aware.
    template <typename Alloc>
    ext_transform_completion_channel_composed(::std::allocator_arg_t,
                                              const Alloc&                                alloc,
                                              ext_transform_completion_channel_composed&& other)
        : inner(::std::make_obj_using_allocator<Inner>(alloc, ::std::move(other.inner))),
          outer(::std::make_obj_using_allocator<Outer>(alloc, ::std::move(other.outer))) {}
    template <typename Alloc>
    ext_transform_completion_channel_composed(::std::allocator_arg_t,
                                              const Alloc&                                     alloc,
                                              const ext_transform_completion_channel_composed& other)
        : inner(::std::make_obj_using_allocator<Inner>(alloc, other.inner)),
          outer(::std::make_obj_using_allocator<Outer>(alloc, other.outer)) {}
};

template <typename Inner, typename Outer>
struct ext_transform_completion_channel_composed_signatures {
    template <typename Sigs>
    consteval auto operator()(Sigs sigs) const {
        using inner_sigs = decltype(Inner{}(sigs));
        static_assert(::beman::execution::detail::valid_completion_signatures<inner_sigs>,
                      "ext_transform_completion_channel: the signature transform has to return completion_signatures");
        return Outer{}(inner_sigs{});
    }
};

// The inner function continues with the outer function which continues with Cont.
template <typename F1, typename F2, typename S1, typename S2, typename ChildSigs, typename Path, typename Cont>
struct ext_transform_completion_channel_dispatch<
    ::beman::execution::detail::ext_transform_completion_channel_composed<F1, F2>,
    ::beman::execution::detail::ext_transform_completion_channel_composed_signatures<S1, S2>,
    ChildSigs,
    Path,
    Cont> {
    using inner_signatures = decltype(S1{}(ChildSigs{}));
    using outer            = ::beman::execution::detail::ext_transform_completion_channel_dispatch<
        F2,
        S2,
        inner_signatures,
        ::beman::execution::detail::ext_transform_completion_channel_path_outer<Path>,
        Cont>;
    using inner = ::beman::execution::detail::ext_transform_completion_channel_dispatch<
        F1,
        S1,
        ChildSigs,
        ::beman::execution::detail::ext_transform_completion_channel_path_inner<Path>,
        outer>;
    using signatures = typename outer::signatures;

    template <typename Root, typename Receiver, typename Tag, typename... Args>
    static constexpr bool can_complete{inner::template can_complete<Root, Receiver, Tag, Args...>};

    template <typename Root, typename Receiver, typename Tag, typename... Args>
    static auto complete(Root& root, Receiver& receiver, Tag, Args&&... args) noexcept -> void {
        inner::complete(root, receiver, Tag(), ::std::forward<Args>(args)...);
    }
};

template <typename Fun, typename SigFun, typename ChildSigs>
struct ext_transform_completion_channel_state {
    using dispatch   = ::beman::execution::detail::ext_transform_completion_channel_dispatch<Fun, SigFun, ChildSigs>;
    using signatures = typename dispatch::signatures;
    Fun fun;
};

/*!
 * \brief The argument type of the only value completion with one argument in Signatures.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 * \internal
 *
 * Functions used with ext_transform_completion_channel can use this helper with
 * the completion_signatures of the receiver to determine the type of a value
 * completion which can't be derived from the arguments of the completion,
 * e.g., the optional to send for a stopped completion in stopped_as_optional.
 */
template <typename>
struct ext_single_value_argument;
template <typename... Sigs>
struct ext_single_value_argument<::beman::execution::completion_signatures<Sigs...>> {
    template <typename Sig>
    struct value {
        using type = ::beman::execution::detail::type_list<>;
    };
    template <typename Arg>
    struct value<::beman::execution::set_value_t(Arg)> {
        using type = ::beman::execution::detail::type_list<Arg>;
    };
    template <typename>
    struct only;
    template <typename Arg>
    struct only<::beman::execution::detail::type_list<Arg>> {
        using type = Arg;
    };
    using type = typename only<::beman::execution::detail::meta::combine<::beman::execution::detail::type_list<>,
                                                                         typename value<Sigs>::type...>>::type;
};
template <typename Signatures>
using ext_single_value_argument_t = typename ::beman::execution::detail::ext_single_value_argument<Signatures>::type;

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

struct ext_transform_completion_channel_t;

template <typename>
inline constexpr bool ext_transform_completion_channel_sender{false};
template <typename Fun, typename SigFun, typename Child>
inline constexpr bool ext_transform_completion_channel_sender<::beman::execution::detail::basic_sender<
    ::beman::execution::detail::ext_transform_completion_channel_t,
    ::beman::execution::detail::ext_transform_completion_channel_data<Fun, SigFun>,
    Child>>{true};

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
    // If the adapted sender is itself an ext_transform_completion_channel
    // sender the two are merged into one sender whose function invokes both
    // functions: a chain of these adaptors needs only one operation state.
    template <::beman::execution::sender                                  Sender,
              ::beman::execution::detail::movable_value                   Fun,
              ::beman::execution::detail::completion_signatures_transform SigFun>
        requires ::beman::execution::detail::ext_transform_completion_channel_sender<::std::remove_cvref_t<Sender>>
    auto operator()(Sender&& sender, Fun&& fun, SigFun) const {
        auto&& data{::std::forward<Sender>(sender).template get<1>()};
        using data_t = ::std::remove_cvref_t<decltype(data)>;
        using composed_t =
            ::beman::execution::detail::ext_transform_completion_channel_composed<typename data_t::fun_type,
                                                                                  ::std::remove_cvref_t<Fun>>;
        using composed_sig_t =
            ::beman::execution::detail::ext_transform_completion_channel_composed_signatures<typename data_t::sig_type,
                                                                                             SigFun>;
        using data_t2 = ::beman::execution::detail::ext_transform_completion_channel_data<composed_t, composed_sig_t>;
        using child_t = ::std::remove_cvref_t<decltype(::std::forward<Sender>(sender).template get<2>())>;
        // constructed directly (instead of using make_sender) to avoid moving the composed function once more
        return ::beman::execution::detail::basic_sender<ext_transform_completion_channel_t, data_t2, child_t>{
            *this,
            data_t2{composed_t(::beman::execution::detail::forward_like<Sender>(data.fun), ::std::forward<Fun>(fun)),
                    composed_sig_t{}},
            ::std::forward<Sender>(sender).template get<2>()};
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
        using child_signatures =
            decltype(::beman::execution::get_completion_signatures<Child,
                                                                   ::beman::execution::detail::fwd_env_t<Env>...>());
        using dispatch =
            ::beman::execution::detail::ext_transform_completion_channel_dispatch<Fun, SigFun, child_signatures>;
        static consteval auto get() { return typename dispatch::signatures{}; }
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
            auto operator()(Sender&& sender, Receiver& receiver) const noexcept {
                auto&& data        = sender.template get<1>();
                using data_t       = ::std::remove_cvref_t<decltype(data)>;
                using child_sigs_t = typename get_signatures<::std::remove_cvref_t<Sender>,
                                                             ::beman::execution::env_of_t<Receiver>>::child_signatures;
                // like the default get_state the function is moved using the receiver's allocator, if any
                return ::beman::execution::detail::ext_transform_completion_channel_state<typename data_t::fun_type,
                                                                                          typename data_t::sig_type,
                                                                                          child_sigs_t>{
                    ::beman::execution::detail::allocator_aware_move(
                        ::beman::execution::detail::forward_like<Sender>(data.fun), receiver)};
            }
        };
        static constexpr auto get_state{get_state_impl{}};

        struct complete_impl {
            template <typename Index, typename State, typename Receiver, typename Tag, typename... Args>
                requires State::dispatch::template
            can_complete<decltype(::std::declval<State&>().fun), Receiver, Tag, Args...> auto
            operator()(Index, State& state, Receiver& receiver, Tag, Args&&... args) const noexcept -> void {
                State::dispatch::complete(state.fun, receiver, Tag(), ::std::forward<Args>(args)...);
            }
        };
        static constexpr auto complete{complete_impl{}};
    };
};
} // namespace beman::execution::detail

template <typename Inner, typename Outer, typename Alloc>
struct std::uses_allocator<::beman::execution::detail::ext_transform_completion_channel_composed<Inner, Outer>, Alloc>
    : ::std::bool_constant<::std::uses_allocator_v<Inner, Alloc> || ::std::uses_allocator_v<Outer, Alloc>> {};

namespace beman::execution {
using ext_transform_completion_channel_t = ::beman::execution::detail::ext_transform_completion_channel_t;
inline constexpr ext_transform_completion_channel_t ext_transform_completion_channel{};

template <typename Fn>
using ext_each_completion_signature_t = ::beman::execution::detail::ext_each_completion_signature_t<Fn>;
inline constexpr ::beman::execution::detail::ext_each_completion_signature_fn ext_each_completion_signature{};
} // namespace beman::execution

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_TRANSFORM_COMPLETION_CHANNEL
