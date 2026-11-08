// tests/beman/execution/exec-ext-transform-completion-channel.test.cpp -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <test/execution.hpp>
#include <test/sender_env.hpp>
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution;
import beman.execution.detail;
#else
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/completion_signatures_of_t.hpp>
#include <beman/execution/detail/connect.hpp>
#include <beman/execution/detail/env.hpp>
#include <beman/execution/detail/forwarding_query.hpp>
#include <beman/execution/detail/get_env.hpp>
#include <beman/execution/detail/just.hpp>
#include <beman/execution/detail/read_env.hpp>
#include <beman/execution/detail/receiver.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_in.hpp>
#include <beman/execution/detail/start.hpp>
#include <beman/execution/detail/sync_wait.hpp>
#include <beman/execution/detail/ext_transform_completion_channel.hpp>
#endif

// ----------------------------------------------------------------------------

namespace {
enum class channel { none, value, error, stopped };

struct result {
    channel     completed{channel::none};
    int         value{};
    std::string error;
};

struct get_value_t : test_std::forwarding_query_t {
    auto operator()(const auto& e) const noexcept -> int { return e.query(*this); }
};

struct receiver {
    using receiver_concept = test_std::receiver_tag;
    result* res;

    auto set_value() && noexcept -> void { this->res->completed = channel::value; }
    auto set_value(int v) && noexcept -> void {
        this->res->completed = channel::value;
        this->res->value     = v;
    }
    auto set_error(std::string e) && noexcept -> void {
        this->res->completed = channel::error;
        this->res->error     = std::move(e);
    }
    auto set_error(int e) && noexcept -> void {
        this->res->completed = channel::error;
        this->res->value     = e;
    }
    auto set_stopped() && noexcept -> void { this->res->completed = channel::stopped; }

    struct env {
        auto query(const get_value_t&) const noexcept -> int { return 42; }
    };
    auto get_env() const noexcept -> env { return {}; }
};

template <typename Sender>
auto run(Sender&& sndr) -> result {
    result res{};
    auto   op{test_std::connect(std::forward<Sender>(sndr), receiver{&res})};
    test_std::start(op);
    return res;
}

// Swap the value and the error channel for completions with an int.
struct swap_fun {
    template <typename Receiver>
    auto operator()(Receiver&& rcvr, auto, test_std::set_value_t, int v) const noexcept -> void {
        test_std::set_error(std::move(rcvr), v);
    }
    template <typename Receiver>
    auto operator()(Receiver&& rcvr, auto, test_std::set_error_t, int e) const noexcept -> void {
        test_std::set_value(std::move(rcvr), e);
    }
};
struct swap_signatures {
    template <typename... Sigs>
    consteval auto operator()(test_std::completion_signatures<Sigs...>) const {
        return test_std::completion_signatures<test_std::set_value_t(int), test_std::set_error_t(int)>{};
    }
};

// Turn a value completion with a negative int into stopped.
// other completions (here: value without an int, error, stopped) are forwarded by the base class
struct negative_as_stopped_fun : test_std::ext_forward_completion {
    using test_std::ext_forward_completion::operator();
    template <typename Receiver>
    auto operator()(Receiver&& rcvr, auto, test_std::set_value_t, int v) const noexcept -> void {
        if (v < 0)
            test_std::set_stopped(std::move(rcvr));
        else
            test_std::set_value(std::move(rcvr), v);
    }
};
struct negative_as_stopped_signatures {
    consteval auto operator()(test_std::set_value_t (*)(int)) const {
        return test_std::completion_signatures<test_std::set_value_t(int), test_std::set_stopped_t()>{};
    }
};

auto test_signatures() -> void {
    using value_sndr = decltype(test_std::just(17));
    using error_sndr = decltype(test_std::just_error(17));

    using swapped = decltype(test_std::ext_transform_completion_channel(
        std::declval<value_sndr>(), swap_fun{}, swap_signatures{}));
    static_assert(test_std::sender<swapped>);
    static_assert(test_std::sender_in<swapped>);
    static_assert(std::same_as<test_std::completion_signatures<test_std::set_value_t(int), test_std::set_error_t(int)>,
                               test_std::completion_signatures_of_t<swapped, test_std::env<>>>);

    using each_value = decltype(test_std::ext_transform_completion_channel(
        std::declval<value_sndr>(),
        negative_as_stopped_fun{},
        test_std::ext_each_completion_signature(negative_as_stopped_signatures{})));
    static_assert(std::same_as<test_std::completion_signatures<test_std::set_value_t(int), test_std::set_stopped_t()>,
                               test_std::completion_signatures_of_t<each_value, test_std::env<>>>);

    // signatures not handled by the per-signature transform are retained
    using each_error = decltype(test_std::ext_transform_completion_channel(
        std::declval<error_sndr>(),
        negative_as_stopped_fun{},
        test_std::ext_each_completion_signature(negative_as_stopped_signatures{})));
    static_assert(std::same_as<test_std::completion_signatures<test_std::set_error_t(int)>,
                               test_std::completion_signatures_of_t<each_error, test_std::env<>>>);
}

auto test_ext_each_completion_signature() -> void {
    constexpr auto transform{test_std::ext_each_completion_signature(negative_as_stopped_signatures{})};
    // duplicate signatures are removed
    static_assert(std::same_as<test_std::completion_signatures<test_std::set_value_t(int),
                                                               test_std::set_stopped_t(),
                                                               test_std::set_error_t(int)>,
                               decltype(transform(test_std::completion_signatures<test_std::set_value_t(int),
                                                                                  test_std::set_stopped_t(),
                                                                                  test_std::set_error_t(int)>{}))>);
    static_assert(
        std::same_as<test_std::completion_signatures<>, decltype(transform(test_std::completion_signatures<>{}))>);
}

struct stopped_as_optional_signatures {
    template <typename T>
    consteval auto operator()(test_std::set_value_t (*)(T)) const {
        return test_std::completion_signatures<test_std::set_value_t(std::optional<T>)>{};
    }
    template <typename E>
    consteval auto operator()(test_std::set_error_t (*)(E)) const {
        return test_std::completion_signatures<test_std::set_error_t(E)>{};
    }
    consteval auto operator()(test_std::set_stopped_t (*)()) const { return test_std::completion_signatures<>{}; }
};

template <typename Transform, typename Signatures>
concept transforms = requires(const Transform& transform) { transform(Signatures{}); };

auto test_ext_all_completion_signature() -> void {
    constexpr auto transform{test_std::ext_all_completion_signature(stopped_as_optional_signatures{})};
    using all_t  = decltype(transform);
    using each_t = decltype(test_std::ext_each_completion_signature(stopped_as_optional_signatures{}));
    static_assert(std::same_as<test_std::completion_signatures<test_std::set_value_t(std::optional<int>),
                                                               test_std::set_error_t(int)>,
                               decltype(transform(test_std::completion_signatures<test_std::set_value_t(int),
                                                                                  test_std::set_stopped_t(),
                                                                                  test_std::set_error_t(int)>{}))>);
    static_assert(
        std::same_as<test_std::completion_signatures<>, decltype(transform(test_std::completion_signatures<>{}))>);
    // a signature the transform doesn't accept makes the transform not invocable (instead of retaining it)
    static_assert(transforms<all_t, test_std::completion_signatures<test_std::set_value_t(int)>>);
    static_assert(not transforms<all_t, test_std::completion_signatures<test_std::set_value_t(int, int)>>);
    static_assert(transforms<each_t, test_std::completion_signatures<test_std::set_value_t(int, int)>>);
}

auto test_declared() -> void {
    using sigs = test_std::completion_signatures<test_std::set_value_t(int), test_std::set_error_t(std::string)>;
    static_assert(test_detail::ext_transform_completion_channel_declared<sigs, test_std::set_value_t, int>);
    static_assert(test_detail::ext_transform_completion_channel_declared<sigs, test_std::set_value_t, int&>);
    static_assert(test_detail::ext_transform_completion_channel_declared<sigs, test_std::set_value_t, const int&>);
    static_assert(not test_detail::ext_transform_completion_channel_declared<sigs, test_std::set_value_t>);
    static_assert(not test_detail::ext_transform_completion_channel_declared<sigs, test_std::set_value_t, long>);
    static_assert(not test_detail::ext_transform_completion_channel_declared<sigs, test_std::set_value_t, int, int>);
    static_assert(test_detail::ext_transform_completion_channel_declared<sigs, test_std::set_error_t, std::string&&>);
    static_assert(not test_detail::ext_transform_completion_channel_declared<sigs, test_std::set_error_t, int>);
    static_assert(not test_detail::ext_transform_completion_channel_declared<sigs, test_std::set_stopped_t>);
}

auto test_runtime() -> void {
    {
        auto res{run(test_std::ext_transform_completion_channel(test_std::just(17), swap_fun{}, swap_signatures{}))};
        ASSERT(res.completed == channel::error);
        ASSERT(res.value == 17);
    }
    {
        auto res{
            run(test_std::just_error(17) | test_std::ext_transform_completion_channel(swap_fun{}, swap_signatures{}))};
        ASSERT(res.completed == channel::value);
        ASSERT(res.value == 17);
    }
    {
        auto res{
            run(test_std::just(-1) | test_std::ext_transform_completion_channel(
                                         negative_as_stopped_fun{},
                                         test_std::ext_each_completion_signature(negative_as_stopped_signatures{})))};
        ASSERT(res.completed == channel::stopped);
    }
    {
        auto res{
            run(test_std::just(1) | test_std::ext_transform_completion_channel(
                                        negative_as_stopped_fun{},
                                        test_std::ext_each_completion_signature(negative_as_stopped_signatures{})))};
        ASSERT(res.completed == channel::value);
        ASSERT(res.value == 1);
    }
    {
        auto [v]{test_std::sync_wait(test_std::just(2) |
                                     test_std::ext_transform_completion_channel(
                                         negative_as_stopped_fun{},
                                         test_std::ext_each_completion_signature(negative_as_stopped_signatures{})))
                     .value()};
        ASSERT(v == 2);
    }
}

auto test_pass_through() -> void {
    auto t{test_std::ext_transform_completion_channel(
        negative_as_stopped_fun{}, test_std::ext_each_completion_signature(negative_as_stopped_signatures{}))};
    {
        auto res{run(test_std::just() | t)};
        ASSERT(res.completed == channel::value);
    }
    {
        auto res{run(test_std::just_error(std::string("error")) | t)};
        ASSERT(res.completed == channel::error);
        ASSERT(res.error == "error");
    }
    {
        auto res{run(test_std::just_stopped() | t)};
        ASSERT(res.completed == channel::stopped);
    }
}

auto test_stateful_fun() -> void {
    // the function may be move-only and is consumed by the completion
    auto fun = [ptr = std::make_unique<std::string>("from fun")]<typename Receiver>(
                   Receiver&& rcvr, auto, test_std::set_value_t) mutable noexcept {
        test_std::set_error(std::move(rcvr), std::move(*ptr));
    };
    struct signatures {
        consteval auto operator()(test_std::set_value_t (*)()) const {
            return test_std::completion_signatures<test_std::set_error_t(std::string)>{};
        }
    };
    auto sndr{test_std::just() | test_std::ext_transform_completion_channel(
                                     std::move(fun), test_std::ext_each_completion_signature(signatures{}))};
    static_assert(std::same_as<test_std::completion_signatures<test_std::set_error_t(std::string)>,
                               test_std::completion_signatures_of_t<decltype(sndr), test_std::env<>>>);
    auto res{run(std::move(sndr))};
    ASSERT(res.completed == channel::error);
    ASSERT(res.error == "from fun");
}

struct env_fun {
    template <typename Receiver>
    auto operator()(Receiver&& rcvr, auto, test_std::set_value_t) const noexcept -> void {
        test_std::set_value(std::move(rcvr), get_value_t{}(test_std::get_env(rcvr)));
    }
};
struct env_signatures {
    consteval auto operator()(test_std::set_value_t (*)()) const {
        return test_std::completion_signatures<test_std::set_value_t(int)>{};
    }
};

auto test_env() -> void {
    // the receiver passed to the function exposes the downstream environment
    auto res{run(test_std::just() | test_std::ext_transform_completion_channel(
                                        env_fun{}, test_std::ext_each_completion_signature(env_signatures{})))};
    ASSERT(res.completed == channel::value);
    ASSERT(res.value == 42);

    // the child sees the (forwarded) downstream environment
    auto dep{
        test_std::read_env(get_value_t{}) |
        test_std::ext_transform_completion_channel(
            negative_as_stopped_fun{}, test_std::ext_each_completion_signature(negative_as_stopped_signatures{}))};
    static_assert(not test_std::sender_in<decltype(dep)>);
    auto res2{run(std::move(dep))};
    ASSERT(res2.completed == channel::value);
    ASSERT(res2.value == 42);
}

auto test_attributes() -> void {
    test::sender_env s{42};
    auto             t{test_std::ext_transform_completion_channel(
        negative_as_stopped_fun{}, test_std::ext_each_completion_signature(negative_as_stopped_signatures{}))};
    test::test_sender_env<true>(
        42,
        test::test_forwardable_attr{},
        test_std::ext_transform_completion_channel(
            s, negative_as_stopped_fun{}, test_std::ext_each_completion_signature(negative_as_stopped_signatures{})));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, s | t);
}
} // namespace

// ----------------------------------------------------------------------------

TEST(exec_ext_transform_completion_channel) {
    static_assert(std::same_as<const test_std::ext_transform_completion_channel_t,
                               decltype(test_std::ext_transform_completion_channel)>);
    test_signatures();
    test_ext_each_completion_signature();
    test_ext_all_completion_signature();
    test_declared();
    test_runtime();
    test_pass_through();
    test_stateful_fun();
    test_env();
    test_attributes();
}
