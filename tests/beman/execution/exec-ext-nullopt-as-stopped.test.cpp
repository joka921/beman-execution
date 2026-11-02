// tests/beman/execution/exec-ext-nullopt-as-stopped.test.cpp         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <test/execution.hpp>
#include <test/sender_env.hpp>
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <optional>
#include <string>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution;
#else
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/completion_signatures_of_t.hpp>
#include <beman/execution/detail/env.hpp>
#include <beman/execution/detail/just.hpp>
#include <beman/execution/detail/ext_nullopt_as_stopped.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sync_wait.hpp>
#endif

// ----------------------------------------------------------------------------

namespace {
template <typename Sender, typename... Sigs>
auto check_signatures(const Sender&) -> void {
    static_assert(std::same_as<test_std::completion_signatures<Sigs...>,
                               test_std::completion_signatures_of_t<Sender, test_std::env<>>>);
}

auto test_ext_nullopt_as_stopped() -> void {
    test_std::sender auto sndr1 = test_std::just(std::optional<int>(17)) | test_std::ext_nullopt_as_stopped();
    check_signatures<decltype(sndr1), test_std::set_value_t(int), test_std::set_stopped_t()>(sndr1);
    auto [v] = test_std::sync_wait(std::move(sndr1)).value();
    ASSERT(v == 17);

    test_std::sender auto sndr2 = test_std::ext_nullopt_as_stopped(test_std::just(std::optional<int>()));
    check_signatures<decltype(sndr2), test_std::set_value_t(int), test_std::set_stopped_t()>(sndr2);
    ASSERT(not test_std::sync_wait(std::move(sndr2)));

    // the value is moved out of the optional
    test_std::sender auto sndr3 = test_std::just(std::optional<std::string>("a string long enough to avoid SSO")) |
                                  test_std::ext_nullopt_as_stopped();
    check_signatures<decltype(sndr3), test_std::set_value_t(std::string), test_std::set_stopped_t()>(sndr3);
    auto [s] = test_std::sync_wait(std::move(sndr3)).value();
    ASSERT(s == "a string long enough to avoid SSO");
}

auto test_ext_nullopt_as_stopped_pass_through() -> void {
    test_std::sender auto sndr1 = test_std::just(17) | test_std::ext_nullopt_as_stopped();
    check_signatures<decltype(sndr1), test_std::set_value_t(int)>(sndr1);
    auto [v] = test_std::sync_wait(std::move(sndr1)).value();
    ASSERT(v == 17);

    // only completions with exactly one optional argument are transformed
    test_std::sender auto sndr2 = test_std::just(std::optional<int>(), 1) | test_std::ext_nullopt_as_stopped();
    check_signatures<decltype(sndr2), test_std::set_value_t(std::optional<int>, int)>(sndr2);

    test_std::sender auto sndr3 = test_std::just_error(std::optional<int>()) | test_std::ext_nullopt_as_stopped();
    check_signatures<decltype(sndr3), test_std::set_error_t(std::optional<int>)>(sndr3);
}

auto test_ext_nullopt_as_stopped_attributes() -> void {
    test::sender_env s{42};
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, test_std::ext_nullopt_as_stopped(s));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, test_std::ext_nullopt_as_stopped(s));
}
} // namespace

// ----------------------------------------------------------------------------

TEST(exec_ext_nullopt_as_stopped) {
    static_assert(std::same_as<const test_std::ext_nullopt_as_stopped_t, decltype(test_std::ext_nullopt_as_stopped)>);
    test_ext_nullopt_as_stopped();
    test_ext_nullopt_as_stopped_pass_through();
    test_ext_nullopt_as_stopped_attributes();
}
