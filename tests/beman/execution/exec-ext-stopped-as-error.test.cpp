// tests/beman/execution/exec-ext-stopped-as-error.test.cpp         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <test/execution.hpp>
#include <test/ext_equivalence.hpp>
#include <test/sender_env.hpp>
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <exception>
#include <string>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution;
#else
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/completion_signatures_of_t.hpp>
#include <beman/execution/detail/env.hpp>
#include <beman/execution/detail/ext_stopped_as_error.hpp>
#include <beman/execution/detail/just.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_in.hpp>
#include <beman/execution/detail/stopped_as_error.hpp>
#include <beman/execution/detail/sync_wait.hpp>
#endif

// ----------------------------------------------------------------------------

namespace {
using test_std::set_error_t;
using test_std::set_stopped_t;
using test_std::set_value_t;

template <typename... Sigs>
auto check(auto error) -> void {
    test::check_equivalent<Sigs...>(
        [error](auto sndr) { return test_std::stopped_as_error(std::move(sndr), error); },
        [error](auto sndr) { return test_std::ext_stopped_as_error(std::move(sndr), error); });
}

auto test_equivalence() -> void {
    check<set_value_t(int)>(42);
    check<set_value_t(int), set_stopped_t()>(42);
    check<set_stopped_t(), set_value_t(int)>(42);
    check<set_value_t(int), set_error_t(int), set_stopped_t()>(42);
    check<set_error_t(std::string), set_stopped_t(), set_value_t(int)>(42);
    check<set_value_t(int), set_stopped_t(), set_error_t(int)>(std::string("stopped"));
    check<set_error_t(std::exception_ptr), set_stopped_t(), set_value_t()>(42);
    // moving the error may throw: set_error_t(exception_ptr) is declared
    check<set_value_t(int), set_stopped_t()>(test::potentially_throwing_move(1));
    check<set_stopped_t(), set_error_t(std::exception_ptr)>(test::potentially_throwing_move(1));
}

auto test_non_dependent() -> void {
    // Difference: see exec-ext-stopped-as-optional.test.cpp
    static_assert(not test_std::sender_in<decltype(test_std::stopped_as_error(test_std::just(1), 2))>);
    static_assert(test_std::sender_in<decltype(test_std::ext_stopped_as_error(test_std::just(1), 2))>);
}

auto test_misc() -> void {
    static_assert(std::same_as<const test_std::ext_stopped_as_error_t, decltype(test_std::ext_stopped_as_error)>);
    ASSERT(test::run_logged(test_std::just_stopped() | test_std::ext_stopped_as_error(42)) == "set_error(int:42&&)");

    test::sender_env s{42};
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, test_std::ext_stopped_as_error(s, 0));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, test_std::ext_stopped_as_error(s, 0));
}
} // namespace

// ----------------------------------------------------------------------------

TEST(exec_ext_stopped_as_error) {
    test_equivalence();
    test_non_dependent();
    test_misc();
}
