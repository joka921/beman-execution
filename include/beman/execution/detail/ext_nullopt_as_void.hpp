// include/beman/execution/detail/ext_nullopt_as_void.hpp             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_NULLOPT_AS_VOID
#define INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_NULLOPT_AS_VOID

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.completion_signatures;
import beman.execution.detail.is_optional;
import beman.execution.detail.sender;
import beman.execution.detail.sender_adaptor_closure;
import beman.execution.detail.set_value;
import beman.execution.detail.ext_transform_completion_channel;
#else
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/is_optional.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_adaptor_closure.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/ext_transform_completion_channel.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {
struct ext_nullopt_as_void_fun {
    template <typename Receiver, ::beman::execution::detail::is_optional Opt>
    auto operator()(Receiver& receiver, ::beman::execution::set_value_t, Opt&& opt) const noexcept -> void {
        if (opt) {
            ::beman::execution::set_value(::std::move(receiver), *::std::forward<Opt>(opt));
        } else {
            ::beman::execution::set_value(::std::move(receiver));
        }
    }
};

struct ext_nullopt_as_void_signatures {
    template <::beman::execution::detail::is_optional Opt>
    consteval auto operator()(::beman::execution::set_value_t (*)(Opt)) const noexcept {
        return ::beman::execution::completion_signatures<::beman::execution::set_value_t(
                                                             typename ::std::remove_cvref_t<Opt>::value_type),
                                                         ::beman::execution::set_value_t()>{};
    }
};

/*!
 * \brief Type of the ext_nullopt_as_void sender adaptor.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 *
 * Value completions with a single std::optional<T> argument complete with set_value(T) if the optional is engaged and
 * with set_value() otherwise. All other completions are forwarded unchanged.
 */
struct ext_nullopt_as_void_t {
    template <::beman::execution::sender Sndr>
    auto operator()(Sndr&& sndr) const {
        return ::beman::execution::ext_transform_completion_channel(
            ::std::forward<Sndr>(sndr),
            ::beman::execution::detail::ext_nullopt_as_void_fun{},
            ::beman::execution::ext_each_completion_signature(
                ::beman::execution::detail::ext_nullopt_as_void_signatures{}));
    }

    auto operator()() const noexcept { return ::beman::execution::detail::make_sender_adaptor(*this); }
};
} // namespace beman::execution::detail

namespace beman::execution {
using ext_nullopt_as_void_t = ::beman::execution::detail::ext_nullopt_as_void_t;
inline constexpr ext_nullopt_as_void_t ext_nullopt_as_void{};
} // namespace beman::execution

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_EXT_NULLOPT_AS_VOID
