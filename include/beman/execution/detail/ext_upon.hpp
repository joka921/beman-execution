// include/beman/execution/detail/ext_upon.hpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_UPON
#define INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_UPON

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <exception>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.completion_signatures;
import beman.execution.detail.ext_transform_completion_channel;
import beman.execution.detail.meta.combine;
import beman.execution.detail.meta.transform;
import beman.execution.detail.meta.unique;
import beman.execution.detail.movable_value;
import beman.execution.detail.sender;
import beman.execution.detail.sender_adaptor_closure;
import beman.execution.detail.set_error;
import beman.execution.detail.set_stopped;
import beman.execution.detail.set_value;
import beman.execution.detail.then;
#else
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/ext_transform_completion_channel.hpp>
#include <beman/execution/detail/meta_combine.hpp>
#include <beman/execution/detail/meta_transform.hpp>
#include <beman/execution/detail/meta_unique.hpp>
#include <beman/execution/detail/movable_value.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_adaptor_closure.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_stopped.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/then.hpp>
#endif

// ----------------------------------------------------------------------------
// ext_upon_error and ext_upon_stopped behave like upon_error and upon_stopped
// but are implemented using ext_transform_completion_channel. The completion
// signatures are computed using the same helpers as then/upon_error/upon_stopped
// and the completion mirrors then_t::impls_for::complete.

namespace beman::execution::detail {
template <typename Tag, typename Fun>
struct ext_upon_fun {
    Fun fun;

    template <typename F>
    explicit ext_upon_fun(::std::in_place_t, F&& f) noexcept(::std::is_nothrow_constructible_v<Fun, F>)
        : fun(::std::forward<F>(f)) {}
    // Allocator-extended constructors: like then the function object is
    // constructed using the receiver's allocator if it is allocator-aware.
    template <typename Alloc>
    ext_upon_fun(::std::allocator_arg_t, const Alloc& alloc, ext_upon_fun&& other)
        : fun(::std::make_obj_using_allocator<Fun>(alloc, ::std::move(other.fun))) {}
    template <typename Alloc>
    ext_upon_fun(::std::allocator_arg_t, const Alloc& alloc, const ext_upon_fun& other)
        : fun(::std::make_obj_using_allocator<Fun>(alloc, other.fun)) {}

    template <typename Receiver, typename... Args>
    auto operator()(Receiver& receiver, Tag, Args&&... args) noexcept -> void {
        try {
            auto invoke = [&] { return ::std::invoke(::std::move(this->fun), ::std::forward<Args>(args)...); };
            if constexpr (::std::same_as<void, decltype(invoke())>) {
                invoke();
                ::beman::execution::set_value(::std::move(receiver));
            } else {
                ::beman::execution::set_value(::std::move(receiver), invoke());
            }
        } catch (...) {
            if constexpr (not noexcept(::std::invoke(::std::move(this->fun), ::std::forward<Args>(args)...))) {
                ::beman::execution::set_error(::std::move(receiver), ::std::current_exception());
            }
        }
    }
};

template <typename Tag, typename Fun>
struct ext_upon_signatures {
    template <typename... Sigs>
    consteval auto operator()(::beman::execution::completion_signatures<Sigs...>) const {
        using child_sigs_t = ::beman::execution::completion_signatures<Sigs...>;
        using type         = ::beman::execution::detail::meta::unique<::beman::execution::detail::meta::combine<
            ::beman::execution::detail::meta::
                transform<::beman::execution::detail::then_transform_t<Fun, Tag>::template transform, child_sigs_t>,
            ::std::conditional_t<
                ::beman::execution::detail::then_exception<Tag, Fun, child_sigs_t>::value,
                ::beman::execution::completion_signatures<::beman::execution::set_error_t(::std::exception_ptr)>,
                ::beman::execution::completion_signatures<>>>>;
        return type{};
    }
};

/*!
 * \brief Type of the ext_upon_error and ext_upon_stopped sender adaptors.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 */
template <typename Tag>
struct ext_upon_t {
    template <::beman::execution::detail::movable_value Fun>
    auto operator()(Fun&& fun) const noexcept(::std::is_nothrow_constructible_v<::std::remove_cvref_t<Fun>, Fun>) {
        return ::beman::execution::detail::make_sender_adaptor(*this, ::std::forward<Fun>(fun));
    }
    template <::beman::execution::sender Sender, ::beman::execution::detail::movable_value Fun>
    auto operator()(Sender&& sender, Fun&& fun) const {
        using fun_t = ::std::remove_cvref_t<Fun>;
        return ::beman::execution::ext_transform_completion_channel(
            ::std::forward<Sender>(sender),
            ::beman::execution::detail::ext_upon_fun<Tag, fun_t>(::std::in_place, ::std::forward<Fun>(fun)),
            ::beman::execution::detail::ext_upon_signatures<Tag, fun_t>{});
    }
};
} // namespace beman::execution::detail

template <typename Tag, typename Fun, typename Alloc>
struct std::uses_allocator<::beman::execution::detail::ext_upon_fun<Tag, Fun>, Alloc>
    : ::std::uses_allocator<Fun, Alloc> {};

namespace beman::execution {
using ext_upon_error_t   = ::beman::execution::detail::ext_upon_t<::beman::execution::set_error_t>;
using ext_upon_stopped_t = ::beman::execution::detail::ext_upon_t<::beman::execution::set_stopped_t>;

inline constexpr ext_upon_error_t   ext_upon_error{};
inline constexpr ext_upon_stopped_t ext_upon_stopped{};
} // namespace beman::execution

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_UPON
