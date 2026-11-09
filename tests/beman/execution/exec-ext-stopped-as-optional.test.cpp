// tests/beman/execution/exec-ext-stopped-as-optional.test.cpp      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <test/execution.hpp>
#include <test/ext_equivalence.hpp>
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
#include <beman/execution/detail/forwarding_query.hpp>
#include <beman/execution/detail/ext_stopped_as_optional.hpp>
#include <beman/execution/detail/just.hpp>
#include <beman/execution/detail/read_env.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_in.hpp>
#include <beman/execution/detail/stopped_as_optional.hpp>
#include <beman/execution/detail/sync_wait.hpp>
#endif

// ----------------------------------------------------------------------------

namespace {
using test_std::set_error_t;
using test_std::set_stopped_t;
using test_std::set_value_t;

template <typename... Sigs>
auto check() -> void {
    test::check_equivalent<Sigs...>([](auto sndr) { return test_std::stopped_as_optional(std::move(sndr)); },
                                    [](auto sndr) { return test_std::ext_stopped_as_optional(std::move(sndr)); });
}

auto test_equivalence() -> void {
    check<set_value_t(int)>();
    check<set_value_t(int), set_stopped_t()>();
    check<set_stopped_t(), set_value_t(int)>();
    check<set_value_t(int), set_error_t(int), set_stopped_t()>();
    check<set_error_t(std::string), set_stopped_t(), set_value_t(std::string)>();
    check<set_stopped_t(), set_error_t(int), set_value_t(const std::string&)>();
    check<set_value_t(int&, double), set_stopped_t()>();
    // constructing the value may throw: set_error_t(exception_ptr) is added and used
    check<set_value_t(test::throwing_copy&), set_stopped_t(), set_error_t(int)>();
    check<set_value_t(test::throwing_copy), set_stopped_t()>();
    check<set_error_t(std::exception_ptr), set_value_t(test::throwing_copy&), set_stopped_t()>();
}

struct get_value_t : test_std::forwarding_query_t {
    auto operator()(const auto& env) const noexcept -> int { return env.query(*this); }
};
struct value_env {
    auto query(const get_value_t&) const noexcept -> int { return 17; }
};

auto test_dependent() -> void {
    // read_env is a dependent sender: the signatures require an environment
    auto existing{test_std::stopped_as_optional(test_std::read_env(get_value_t{}))};
    auto ext{test_std::ext_stopped_as_optional(test_std::read_env(get_value_t{}))};
    static_assert(std::same_as<test_std::completion_signatures_of_t<decltype(existing), value_env>,
                               test_std::completion_signatures_of_t<decltype(ext), value_env>>);
    ASSERT(test::run_logged(std::move(existing), value_env{}) == test::run_logged(std::move(ext), value_env{}));
}

auto test_non_dependent() -> void {
    // Difference: the lowered stopped_as_optional only provides signatures
    // when an environment is provided while ext_stopped_as_optional can
    // determine the signatures of a non-dependent sender without environment.
    static_assert(not test_std::sender_in<decltype(test_std::stopped_as_optional(test_std::just(1)))>);
    static_assert(test_std::sender_in<decltype(test_std::ext_stopped_as_optional(test_std::just(1)))>);
}

auto test_misc() -> void {
    static_assert(
        std::same_as<const test_std::ext_stopped_as_optional_t, decltype(test_std::ext_stopped_as_optional)>);
    using sndr = test::choice_sender<set_value_t(int), set_stopped_t()>;
    auto [v1]  = test_std::sync_wait(sndr{0} | test_std::ext_stopped_as_optional()).value();
    ASSERT(v1 == 17);
    auto [v2] = test_std::sync_wait(sndr{1} | test_std::ext_stopped_as_optional()).value();
    ASSERT(!v2);

    test::sender_env s{42};
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, test_std::ext_stopped_as_optional(s));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, test_std::ext_stopped_as_optional(s));
}
} // namespace

// ----------------------------------------------------------------------------

TEST(exec_ext_stopped_as_optional) {
    test_equivalence();
    test_dependent();
    test_non_dependent();
    test_misc();
}
