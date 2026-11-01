// tests/beman/execution/exec-ext-unexpected-as-error.test.cpp        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <version>
#include <test/execution.hpp>
#include <test/sender_env.hpp>
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#if defined(__cpp_lib_expected)
#include <expected>
#endif
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
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sync_wait.hpp>
#include <beman/execution/detail/ext_unexpected_as_error.hpp>
#endif

// ----------------------------------------------------------------------------

#if defined(__cpp_lib_expected)
namespace {
auto test_ext_unexpected_as_error() -> void {
    using expected = std::expected<int, std::string>;

    test_std::sender auto sndr1 = test_std::just(expected(17)) | test_std::ext_unexpected_as_error();
    static_assert(
        std::same_as<test_std::completion_signatures<test_std::set_value_t(int), test_std::set_error_t(std::string)>,
                     test_std::completion_signatures_of_t<decltype(sndr1), test_std::env<>>>);
    auto [v] = test_std::sync_wait(std::move(sndr1)).value();
    ASSERT(v == 17);

    try {
        test_std::sync_wait(
            test_std::ext_unexpected_as_error(test_std::just(expected(std::unexpected(std::string("error"))))));
        ASSERT_UNREACHABLE();
    } catch (const std::string& e) {
        ASSERT(e == "error");
    }
}

auto test_ext_unexpected_as_error_void() -> void {
    using expected = std::expected<void, int>;

    test_std::sender auto sndr1 = test_std::just(expected()) | test_std::ext_unexpected_as_error();
    static_assert(std::same_as<test_std::completion_signatures<test_std::set_value_t(), test_std::set_error_t(int)>,
                               test_std::completion_signatures_of_t<decltype(sndr1), test_std::env<>>>);
    ASSERT(test_std::sync_wait(std::move(sndr1)));

    try {
        test_std::sync_wait(test_std::just(expected(std::unexpected(42))) | test_std::ext_unexpected_as_error());
        ASSERT_UNREACHABLE();
    } catch (int e) {
        ASSERT(e == 42);
    }
}

auto test_ext_unexpected_as_error_pass_through() -> void {
    test_std::sender auto sndr = test_std::just(17) | test_std::ext_unexpected_as_error();
    static_assert(std::same_as<test_std::completion_signatures<test_std::set_value_t(int)>,
                               test_std::completion_signatures_of_t<decltype(sndr), test_std::env<>>>);
    auto [v] = test_std::sync_wait(std::move(sndr)).value();
    ASSERT(v == 17);
}

auto test_ext_unexpected_as_error_attributes() -> void {
    test::sender_env s{42};
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, test_std::ext_unexpected_as_error(s));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, test_std::ext_unexpected_as_error(s));
}
} // namespace
#endif

// ----------------------------------------------------------------------------

TEST(exec_ext_unexpected_as_error) {
#if defined(__cpp_lib_expected)
    static_assert(
        std::same_as<const test_std::ext_unexpected_as_error_t, decltype(test_std::ext_unexpected_as_error)>);
    test_ext_unexpected_as_error();
    test_ext_unexpected_as_error_void();
    test_ext_unexpected_as_error_pass_through();
    test_ext_unexpected_as_error_attributes();
#endif
}
