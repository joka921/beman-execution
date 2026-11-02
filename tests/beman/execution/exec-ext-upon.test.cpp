// tests/beman/execution/exec-ext-upon.test.cpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <test/execution.hpp>
#include <test/ext_equivalence.hpp>
#include <test/sender_env.hpp>
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <memory>
#include <memory_resource>
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
#include <beman/execution/detail/ext_upon.hpp>
#include <beman/execution/detail/get_allocator.hpp>
#include <beman/execution/detail/just.hpp>
#include <beman/execution/detail/prop.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sync_wait.hpp>
#include <beman/execution/detail/then.hpp>
#endif

// ----------------------------------------------------------------------------

namespace {
using test_std::set_error_t;
using test_std::set_stopped_t;
using test_std::set_value_t;

template <typename... Sigs>
auto check_upon(auto fun) -> void {
    test::check_equivalent<Sigs...>([fun](auto sndr) { return test_std::upon_error(std::move(sndr), fun); },
                                    [fun](auto sndr) { return test_std::ext_upon_error(std::move(sndr), fun); });
    test::check_equivalent<Sigs...>([fun](auto sndr) { return test_std::upon_stopped(std::move(sndr), fun); },
                                    [fun](auto sndr) { return test_std::ext_upon_stopped(std::move(sndr), fun); });
}

struct to_string {
    auto operator()(int e) const noexcept -> std::string { return "error " + std::to_string(e); }
    auto operator()() const noexcept -> std::string { return "stopped"; }
};
struct throws {
    auto operator()(int e) const -> int {
        if (e == 17)
            throw std::runtime_error("upon");
        return e;
    }
    auto operator()() const -> int { throw std::runtime_error("upon"); }
};
struct returns_void {
    auto operator()(int) const noexcept -> void {}
    auto operator()() const noexcept -> void {}
};
struct returns_ref {
    int* ptr;
    auto operator()(int) const noexcept -> int& { return *ptr; }
    auto operator()() const noexcept -> int& { return *ptr; }
};

auto test_equivalence() -> void {
    check_upon<set_value_t(int), set_error_t(int), set_stopped_t()>(to_string{});
    check_upon<set_error_t(int), set_stopped_t(), set_value_t(std::string)>(to_string{});
    check_upon<set_stopped_t(), set_value_t(int&), set_error_t(int), set_value_t()>(to_string{});
    check_upon<set_value_t(int), set_error_t(int), set_stopped_t()>(throws{});
    check_upon<set_error_t(int), set_value_t(int), set_stopped_t()>(returns_void{});
    int value{3};
    check_upon<set_error_t(int), set_value_t(int), set_stopped_t()>(returns_ref{&value});
    // no matching completion: the completions are forwarded unchanged
    check_upon<set_value_t(int)>(to_string{});
}

// An allocator-aware function object recording the allocator it was constructed with.
struct allocator_aware_fun {
    using allocator_type = std::pmr::polymorphic_allocator<>;
    std::pmr::memory_resource* resource{std::pmr::get_default_resource()};

    allocator_aware_fun()                           = default;
    allocator_aware_fun(const allocator_aware_fun&) = default;
    allocator_aware_fun(allocator_aware_fun&&)      = default;
    allocator_aware_fun(std::allocator_arg_t, const allocator_type& alloc, const allocator_aware_fun&)
        : resource(alloc.resource()) {}
    allocator_aware_fun(std::allocator_arg_t, const allocator_type& alloc, allocator_aware_fun&&)
        : resource(alloc.resource()) {}

    auto operator()(int) const noexcept -> std::string {
        return this->resource == std::pmr::get_default_resource() ? "default" : "custom";
    }
};

auto test_allocator() -> void {
    std::pmr::monotonic_buffer_resource resource;
    auto env{test_std::prop(test_std::get_allocator, std::pmr::polymorphic_allocator<>(&resource))};
    auto existing{test::run_logged(test_std::just_error(1) | test_std::upon_error(allocator_aware_fun{}), env)};
    auto ext{test::run_logged(test_std::just_error(1) | test_std::ext_upon_error(allocator_aware_fun{}), env)};
    ASSERT(existing == ext);
    ASSERT(ext.find("custom") != std::string::npos);
}

auto test_misc() -> void {
    static_assert(std::same_as<const test_std::ext_upon_error_t, decltype(test_std::ext_upon_error)>);
    static_assert(std::same_as<const test_std::ext_upon_stopped_t, decltype(test_std::ext_upon_stopped)>);
    auto [v] = test_std::sync_wait(test_std::just_stopped() | test_std::ext_upon_stopped([] { return 42; })).value();
    ASSERT(v == 42);

    test::sender_env s{42};
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, test_std::ext_upon_error(s, to_string{}));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, test_std::ext_upon_error(s, to_string{}));
}
} // namespace

// ----------------------------------------------------------------------------

TEST(exec_ext_upon) {
    test_equivalence();
    test_allocator();
    test_misc();
}
