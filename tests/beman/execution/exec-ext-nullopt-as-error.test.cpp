// tests/beman/execution/exec-ext-nullopt-as-error.test.cpp           -*-C++-*-
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
#include <beman/execution/detail/connect.hpp>
#include <beman/execution/detail/env.hpp>
#include <beman/execution/detail/just.hpp>
#include <beman/execution/detail/ext_nullopt_as_error.hpp>
#include <beman/execution/detail/receiver.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/start.hpp>
#include <beman/execution/detail/sync_wait.hpp>
#endif

// ----------------------------------------------------------------------------

namespace {
struct error_receiver {
    using receiver_concept = test_std::receiver_tag;
    std::string* error;

    auto set_value(int) && noexcept -> void {}
    auto set_error(std::string e) && noexcept -> void { *this->error = std::move(e); }
};

auto test_ext_nullopt_as_error() -> void {
    test_std::sender auto sndr1 =
        test_std::just(std::optional<int>(17)) | test_std::ext_nullopt_as_error(std::string("none"));
    static_assert(
        std::same_as<test_std::completion_signatures<test_std::set_value_t(int), test_std::set_error_t(std::string)>,
                     test_std::completion_signatures_of_t<decltype(sndr1), test_std::env<>>>);
    std::string error;
    auto        op1{test_std::connect(std::move(sndr1), error_receiver{&error})};
    test_std::start(op1);
    ASSERT(error.empty());

    test_std::sender auto sndr2 =
        test_std::ext_nullopt_as_error(test_std::just(std::optional<int>()), std::string("none"));
    auto op2{test_std::connect(std::move(sndr2), error_receiver{&error})};
    test_std::start(op2);
    ASSERT(error == "none");
}

auto test_ext_nullopt_as_error_sync_wait() -> void {
    test_std::sender auto sndr = test_std::just(std::optional<int>(17)) | test_std::ext_nullopt_as_error(42);
    static_assert(std::same_as<test_std::completion_signatures<test_std::set_value_t(int), test_std::set_error_t(int)>,
                               test_std::completion_signatures_of_t<decltype(sndr), test_std::env<>>>);
    auto [v] = test_std::sync_wait(std::move(sndr)).value();
    ASSERT(v == 17);

    try {
        test_std::sync_wait(test_std::just(std::optional<int>()) | test_std::ext_nullopt_as_error(42));
        ASSERT_UNREACHABLE();
    } catch (int e) {
        ASSERT(e == 42);
    }
}

auto test_ext_nullopt_as_error_attributes() -> void {
    test::sender_env s{42};
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, test_std::ext_nullopt_as_error(s, 0));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, test_std::ext_nullopt_as_error(s, 0));
}
} // namespace

// ----------------------------------------------------------------------------

TEST(exec_ext_nullopt_as_error) {
    static_assert(std::same_as<const test_std::ext_nullopt_as_error_t, decltype(test_std::ext_nullopt_as_error)>);
    test_ext_nullopt_as_error();
    test_ext_nullopt_as_error_sync_wait();
    test_ext_nullopt_as_error_attributes();
}
