// tests/beman/execution/exec-ext-error-as-unexpected.test.cpp        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <version>
#include <test/execution.hpp>
#include <test/ext_equivalence.hpp>
#include <test/sender_env.hpp>
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <exception>
#if defined(__cpp_lib_expected)
#include <expected>
#endif
#include <stdexcept>
#include <string>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution;
#else
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/completion_signatures_of_t.hpp>
#include <beman/execution/detail/env.hpp>
#include <beman/execution/detail/ext_error_as_unexpected.hpp>
#include <beman/execution/detail/ext_unexpected_as_error.hpp>
#include <beman/execution/detail/get_completion_signatures.hpp>
#include <beman/execution/detail/just.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sync_wait.hpp>
#include <beman/execution/detail/then.hpp>
#endif

// ----------------------------------------------------------------------------

#if defined(__cpp_lib_expected)
namespace {
using test_std::set_error_t;
using test_std::set_stopped_t;
using test_std::set_value_t;

template <typename Sender>
using sigs_of = test_std::completion_signatures_of_t<Sender, test_std::env<>>;

auto test_ext_error_as_unexpected() -> void {
    using child    = test::choice_sender<set_value_t(int), set_error_t(std::string)>;
    using expected = std::expected<int, std::string>;

    static_assert(std::same_as<test_std::completion_signatures<set_value_t(expected)>,
                               sigs_of<decltype(test_std::ext_error_as_unexpected(child{}))>>);
    // the signatures don't depend on an environment
    static_assert(
        std::same_as<
            test_std::completion_signatures<set_value_t(expected)>,
            decltype(test_std::get_completion_signatures<decltype(child{} | test_std::ext_error_as_unexpected())>())>);

    auto [value] = test_std::sync_wait(child{0} | test_std::ext_error_as_unexpected()).value();
    ASSERT(value == expected(17));

    auto [error] = test_std::sync_wait(test_std::ext_error_as_unexpected(child{1})).value();
    ASSERT(not error.has_value());
    ASSERT(error.error() == test::make_value<std::string>());
}

auto test_ext_error_as_unexpected_void_and_stopped() -> void {
    using child    = test::choice_sender<set_stopped_t(), set_value_t(), set_error_t(int)>;
    using expected = std::expected<void, int>;
    using sender   = decltype(child{} | test_std::ext_error_as_unexpected());

    // the stopped completion is forwarded unchanged
    static_assert(
        std::same_as<test_std::completion_signatures<set_value_t(expected), set_stopped_t()>, sigs_of<sender>>);
    ASSERT(test::run_logged(child{0} | test_std::ext_error_as_unexpected()) == "set_stopped()");

    auto [value] = test_std::sync_wait(child{1} | test_std::ext_error_as_unexpected()).value();
    ASSERT(value.has_value());

    auto [error] = test_std::sync_wait(child{2} | test_std::ext_error_as_unexpected()).value();
    ASSERT(error == expected(std::unexpected(17)));
}

auto test_ext_error_as_unexpected_throwing() -> void {
    // copying the value (or the error) into the expected may throw
    using child    = test::choice_sender<set_value_t(test::throwing_copy&), set_error_t(const std::string&)>;
    using expected = std::expected<test::throwing_copy, std::string>;
    using sender   = decltype(child{} | test_std::ext_error_as_unexpected());

    static_assert(std::same_as<test_std::completion_signatures<set_value_t(expected), set_error_t(std::exception_ptr)>,
                               sigs_of<sender>>);
    ASSERT(test::run_logged(child{0} | test_std::ext_error_as_unexpected()) == "set_error(exception:throwing_copy&&)");

    auto [error] = test_std::sync_wait(child{1} | test_std::ext_error_as_unexpected()).value();
    ASSERT(error.error() == test::make_value<std::string>());
}

auto test_ext_error_as_unexpected_exception() -> void {
    // a typical use: capture the exception of a function as expected<T, exception_ptr>
    auto fun = [](int value) {
        if (value < 0)
            throw std::runtime_error("negative");
        return value;
    };
    using expected = std::expected<int, std::exception_ptr>;

    test_std::sender auto sndr = test_std::just(17) | test_std::then(fun) | test_std::ext_error_as_unexpected();
    static_assert(std::same_as<test_std::completion_signatures<set_value_t(expected)>, sigs_of<decltype(sndr)>>);
    auto [value] = test_std::sync_wait(std::move(sndr)).value();
    ASSERT(value == 17);

    auto [error] =
        test_std::sync_wait(test_std::just(-1) | test_std::then(fun) | test_std::ext_error_as_unexpected()).value();
    ASSERT(test::describe(error.error()) == "exception:negative");
}

auto test_ext_error_as_unexpected_round_trip() -> void {
    // ext_unexpected_as_error undoes ext_error_as_unexpected
    using child = test::choice_sender<set_value_t(int), set_error_t(std::string), set_stopped_t()>;
    auto round_trip{[](auto sndr) {
        return std::move(sndr) | test_std::ext_error_as_unexpected() | test_std::ext_unexpected_as_error();
    }};

    static_assert(std::same_as<sigs_of<child>, sigs_of<decltype(round_trip(child{}))>>);
    for (std::size_t which{}; which != 3u; ++which) {
        ASSERT(test::run_logged(child{which}) == test::run_logged(round_trip(child{which})));
    }
}

struct sender_env_with_error : test::sender_env {
    using completion_signatures = test_std::completion_signatures<set_value_t(), set_error_t(int)>;
    template <typename, typename...>
    static consteval auto get_completion_signatures() -> completion_signatures {
        return {};
    }
};

auto test_ext_error_as_unexpected_attributes() -> void {
    sender_env_with_error s{{42}};
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, test_std::ext_error_as_unexpected(s));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, test_std::ext_error_as_unexpected(s));
}
} // namespace
#endif

// ----------------------------------------------------------------------------

TEST(exec_ext_error_as_unexpected) {
#if defined(__cpp_lib_expected)
    static_assert(
        std::same_as<const test_std::ext_error_as_unexpected_t, decltype(test_std::ext_error_as_unexpected)>);
    test_ext_error_as_unexpected();
    test_ext_error_as_unexpected_void_and_stopped();
    test_ext_error_as_unexpected_throwing();
    test_ext_error_as_unexpected_exception();
    test_ext_error_as_unexpected_round_trip();
    test_ext_error_as_unexpected_attributes();
#endif
}
