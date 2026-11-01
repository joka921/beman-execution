// tests/beman/execution/exec-ext-nullopt-as-void.test.cpp            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <test/execution.hpp>
#include <test/sender_env.hpp>
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution;
#else
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/completion_signatures_of_t.hpp>
#include <beman/execution/detail/env.hpp>
#include <beman/execution/detail/just.hpp>
#include <beman/execution/detail/ext_nullopt_as_void.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sync_wait_with_variant.hpp>
#endif

// ----------------------------------------------------------------------------

namespace {
auto test_ext_nullopt_as_void() -> void {
    test_std::sender auto sndr1 = test_std::just(std::optional<int>(17)) | test_std::ext_nullopt_as_void();
    static_assert(std::same_as<test_std::completion_signatures<test_std::set_value_t(int), test_std::set_value_t()>,
                               test_std::completion_signatures_of_t<decltype(sndr1), test_std::env<>>>);
    auto r1 = test_std::sync_wait_with_variant(std::move(sndr1)).value();
    ASSERT(std::holds_alternative<std::tuple<int>>(r1));
    ASSERT(std::get<std::tuple<int>>(r1) == std::tuple<int>(17));

    test_std::sender auto sndr2 = test_std::ext_nullopt_as_void(test_std::just(std::optional<int>()));
    auto                  r2    = test_std::sync_wait_with_variant(std::move(sndr2)).value();
    ASSERT(std::holds_alternative<std::tuple<>>(r2));
}

auto test_ext_nullopt_as_void_pass_through() -> void {
    test_std::sender auto sndr = test_std::just_stopped() | test_std::ext_nullopt_as_void();
    static_assert(std::same_as<test_std::completion_signatures<test_std::set_stopped_t()>,
                               test_std::completion_signatures_of_t<decltype(sndr), test_std::env<>>>);
}

auto test_ext_nullopt_as_void_attributes() -> void {
    test::sender_env s{42};
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, test_std::ext_nullopt_as_void(s));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, test_std::ext_nullopt_as_void(s));
}
} // namespace

// ----------------------------------------------------------------------------

TEST(exec_ext_nullopt_as_void) {
    static_assert(std::same_as<const test_std::ext_nullopt_as_void_t, decltype(test_std::ext_nullopt_as_void)>);
    test_ext_nullopt_as_void();
    test_ext_nullopt_as_void_pass_through();
    test_ext_nullopt_as_void_attributes();
}
