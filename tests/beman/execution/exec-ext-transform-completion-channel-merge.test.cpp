// tests/beman/execution/exec-ext-transform-completion-channel-merge.test.cpp -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <test/execution.hpp>
#include <test/ext_equivalence.hpp>
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <exception>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution;
import beman.execution.detail;
#else
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/completion_signatures_of_t.hpp>
#include <beman/execution/detail/connect_result_t.hpp>
#include <beman/execution/detail/env.hpp>
#include <beman/execution/detail/ext_nullopt_as_error.hpp>
#include <beman/execution/detail/ext_nullopt_as_stopped.hpp>
#include <beman/execution/detail/ext_stopped_as_optional.hpp>
#include <beman/execution/detail/ext_transform_completion_channel.hpp>
#include <beman/execution/detail/ext_upon.hpp>
#include <beman/execution/detail/forwarding_query.hpp>
#include <beman/execution/detail/get_env.hpp>
#include <beman/execution/detail/just.hpp>
#include <beman/execution/detail/make_sender.hpp>
#include <beman/execution/detail/prop.hpp>
#include <beman/execution/detail/sync_wait.hpp>
#include <beman/execution/detail/then.hpp>
#include <beman/execution/detail/write_env.hpp>
#endif

// ----------------------------------------------------------------------------
// ext_transform_completion_channel merges a chain of ext_transform_completion_channel
// senders into one sender. The tests compare such merged chains with the
// equivalent nested senders, i.e., the senders without merging.

namespace {
using test_std::set_error_t;
using test_std::set_stopped_t;
using test_std::set_value_t;

// Apply the adaptor to inner without merging: the result is an
// ext_transform_completion_channel sender whose child is inner.
template <typename Inner, typename Adaptor>
auto nest(Inner&& inner, Adaptor adaptor) {
    auto data{(test_std::just() | adaptor).template get<1>()};
    return test_detail::make_sender(
        test_std::ext_transform_completion_channel, std::move(data), std::forward<Inner>(inner));
}
template <typename Inner>
auto nest_all(Inner&& inner) -> Inner {
    return std::forward<Inner>(inner);
}
template <typename Inner, typename Adaptor, typename... Adaptors>
auto nest_all(Inner&& inner, Adaptor adaptor, Adaptors... adaptors) {
    return nest_all(nest(std::forward<Inner>(inner), adaptor), adaptors...);
}
template <typename Inner>
auto merge_all(Inner&& inner) -> Inner {
    return std::forward<Inner>(inner);
}
template <typename Inner, typename Adaptor, typename... Adaptors>
auto merge_all(Inner&& inner, Adaptor adaptor, Adaptors... adaptors) {
    return merge_all(std::forward<Inner>(inner) | adaptor, adaptors...);
}

template <typename... Sigs>
auto check_merged(auto... adaptors) -> void {
    test::check_equivalent<Sigs...>([=](auto sndr) { return nest_all(std::move(sndr), adaptors...); },
                                    [=](auto sndr) { return merge_all(std::move(sndr), adaptors...); });
}

struct throws_on_17 {
    auto operator()(int v) const -> int {
        if (v == 17)
            throw std::runtime_error("17");
        return v + 1;
    }
};
struct recover {
    auto operator()(int e) const noexcept -> int { return -e; }
    auto operator()(const std::exception_ptr&) const noexcept -> int { return -1; }
};
struct to_string {
    auto operator()(int v) const -> std::string { return std::to_string(v); }
    auto operator()() const -> std::string { return "stopped"; }
};
struct to_optional {
    auto operator()(int v) const noexcept -> std::optional<int> {
        return v == 17 ? std::nullopt : std::optional<int>(v);
    }
    auto operator()(double v) const noexcept -> std::optional<int> { return static_cast<int>(v * 2); }
};
struct twice {
    auto operator()(int v) const noexcept -> int { return 2 * v; }
};

auto test_merged_type() -> void {
    using merged = decltype(test_std::just(1) | test_std::ext_then(twice{}) | test_std::ext_then(twice{}) |
                            test_std::ext_upon_error(recover{}));
    static_assert(test_detail::ext_transform_completion_channel_sender<merged>);
    // the child of the merged sender is the just sender
    static_assert(not test_detail::ext_transform_completion_channel_sender<
                  std::remove_cvref_t<decltype(std::declval<merged>().template get<2>())>>);

    using nested = decltype(nest_all(test_std::just(1), test_std::ext_then(twice{}), test_std::ext_then(twice{})));
    static_assert(test_detail::ext_transform_completion_channel_sender<
                  std::remove_cvref_t<decltype(std::declval<nested>().template get<2>())>>);
    // the merged sender needs only one operation state
    static_assert(sizeof(test_std::connect_result_t<merged, test::logging_receiver<>>) <
                  sizeof(test_std::connect_result_t<nested, test::logging_receiver<>>));
}

auto test_then_upon_chain() -> void {
    // all channels, exceptions thrown by an inner function, and pass-through completions
    auto chain = [](auto then, auto upon_error, auto upon_stopped) {
        return [=](auto sndr) {
            return std::move(sndr) | then(throws_on_17{}) | upon_error(recover{}) | then(to_string{}) |
                   upon_stopped(to_string{});
        };
    };
    using child = test::choice_sender<set_value_t(int), set_error_t(int), set_stopped_t()>;
    auto existing{chain(test_std::then, test_std::upon_error, test_std::upon_stopped)};
    auto ext{chain(test_std::ext_then, test_std::ext_upon_error, test_std::ext_upon_stopped)};
    test::check_equivalent<set_value_t(int), set_error_t(int), set_stopped_t()>(existing, ext);
    ASSERT(test::run_logged(ext(child{0})) == "set_value(string:-1&&,)");

    check_merged<set_value_t(int), set_error_t(int), set_stopped_t()>(test_std::ext_then(throws_on_17{}),
                                                                      test_std::ext_upon_error(recover{}),
                                                                      test_std::ext_then(to_string{}),
                                                                      test_std::ext_upon_stopped(to_string{}));
}

auto test_optional_chain() -> void {
    // stateful functions (the error of ext_nullopt_as_error) and functions using the receiver's signatures
    check_merged<set_value_t(int), set_value_t(double), set_error_t(int), set_stopped_t()>(
        test_std::ext_then(to_optional{}),
        test_std::ext_nullopt_as_stopped(),
        test_std::ext_then(twice{}),
        test_std::ext_stopped_as_optional(),
        test_std::ext_nullopt_as_error(std::string("empty")));
    check_merged<set_value_t(int), set_stopped_t(), set_error_t(std::string)>(test_std::ext_then(to_optional{}),
                                                                              test_std::ext_nullopt_as_stopped(),
                                                                              test_std::ext_then(to_optional{}),
                                                                              test_std::ext_nullopt_as_stopped(),
                                                                              test_std::ext_then(to_optional{}),
                                                                              test_std::ext_nullopt_as_stopped());
}

struct get_value_t : test_std::forwarding_query_t {
    auto operator()(const auto& e) const noexcept -> int { return e.query(*this); }
};
struct non_forwarding_t {
    template <typename Env>
        requires requires(const Env& e, const non_forwarding_t& q) { e.query(q); }
    auto operator()(const Env& e) const noexcept -> int {
        return e.query(*this);
    }
};
// Adds the value of get_value and whether non_forwarding is visible to the value.
struct env_fun {
    template <typename Receiver>
    auto operator()(Receiver&& rcvr, auto, set_value_t, int v) const noexcept -> void {
        auto env{test_std::get_env(rcvr)};
        test_std::set_value(std::move(rcvr),
                            10 * v + get_value_t{}(env) + (requires { non_forwarding_t{}(env); } ? 100 : 0));
    }
};
struct env_signatures {
    template <typename Sigs>
    consteval auto operator()(Sigs) const {
        return test_std::completion_signatures<set_value_t(int)>{};
    }
};

auto test_env() -> void {
    // like for nested senders the inner functions see a forwarding environment
    auto adaptor{test_std::ext_transform_completion_channel(env_fun{}, env_signatures{})};
    auto env{test_std::env{test_std::prop(get_value_t{}, 1), test_std::prop(non_forwarding_t{}, 2)}};
    auto merged{test::run_logged(test_std::just(0) | adaptor | adaptor | adaptor, env)};
    auto nested{test::run_logged(nest_all(test_std::just(0), adaptor, adaptor, adaptor), env)};
    ASSERT(merged == nested);
    ASSERT(merged == "set_value(int:211&&,)");
}

auto test_write_env() -> void {
    // an environment change in the middle of a chain stops the merging: the
    // functions before write_env see the written (non-forwarding) queries
    // exactly like nested senders do
    auto adaptor{test_std::ext_transform_completion_channel(env_fun{}, env_signatures{})};
    auto written{test_std::env{test_std::prop(get_value_t{}, 5), test_std::prop(non_forwarding_t{}, 3)}};
    auto env{test_std::env{test_std::prop(get_value_t{}, 1), test_std::prop(non_forwarding_t{}, 2)}};

    auto chain{test_std::just(0) | adaptor | adaptor | test_std::write_env(written) | adaptor | adaptor};
    // the outer part is merged and its child is the write_env sender ...
    using write_env_sender = std::remove_cvref_t<decltype(std::move(chain).template get<2>())>;
    static_assert(not test_detail::ext_transform_completion_channel_sender<write_env_sender>);
    // ... whose child is the merged inner part
    static_assert(test_detail::ext_transform_completion_channel_sender<
                  std::remove_cvref_t<decltype(std::declval<write_env_sender>().template get<2>())>>);

    auto nested{
        nest_all(nest_all(test_std::just(0), adaptor, adaptor) | test_std::write_env(written), adaptor, adaptor)};
    auto merged_log{test::run_logged(std::move(chain), env)};
    ASSERT(merged_log == test::run_logged(std::move(nested), env));
    // inner part: 5 (written value, non-forwarding hidden), then 10 * 5 + 5 + 100 (the
    // outermost function before write_env sees the written non-forwarding query);
    // outer part: 10 * 155 + 1, then 10 * 1551 + 1 + 100
    ASSERT(merged_log == "set_value(int:15611&&,)");
}

auto test_stateless() -> void {
    // stateless functions (empty and trivially copyable) take no space in a merged sender
    auto sndr{test_std::just(1) | test_std::ext_then(twice{}) | test_std::ext_then(twice{}) |
              test_std::ext_then(twice{}) | test_std::ext_then(twice{})};
    using fun_t = std::remove_cvref_t<decltype(sndr.template get<1>().fun)>;
    static_assert(test_detail::ext_transform_completion_channel_stateless<fun_t>);
    // a stateful function is stored
    auto stateful{test_std::just(1) | test_std::ext_then(twice{}) | test_std::ext_nullopt_as_error(std::string("e"))};
    static_assert(not test_detail::ext_transform_completion_channel_stateless<
                  std::remove_cvref_t<decltype(stateful.template get<1>().fun)>>);
    auto [v] = test_std::sync_wait(std::move(sndr)).value();
    ASSERT(v == 16);
}
} // namespace

// ----------------------------------------------------------------------------

TEST(exec_ext_transform_completion_channel_merge) {
    test_merged_type();
    test_then_upon_chain();
    test_optional_chain();
    test_env();
    test_write_env();
    test_stateless();
}
