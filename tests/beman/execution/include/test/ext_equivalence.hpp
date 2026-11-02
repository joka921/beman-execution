// tests/beman/execution/include/test/ext_equivalence.hpp           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_TESTS_BEMAN_EXECUTION_INCLUDE_TEST_EXT_EQUIVALENCE
#define INCLUDED_TESTS_BEMAN_EXECUTION_INCLUDE_TEST_EXT_EQUIVALENCE

// Helpers to check that an ext_ adaptor implemented using
// ext_transform_completion_channel behaves like the corresponding standard
// adaptor: same completion signatures (including their order) and same
// completions for each possible completion of the adapted sender.

#include <version>
#include <test/execution.hpp>
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <cstddef>
#include <exception>
#if defined(__cpp_lib_expected)
#include <expected>
#endif
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution;
#else
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/completion_signatures_of_t.hpp>
#include <beman/execution/detail/connect.hpp>
#include <beman/execution/detail/env.hpp>
#include <beman/execution/detail/operation_state.hpp>
#include <beman/execution/detail/receiver.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_stopped.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/start.hpp>
#endif

// ----------------------------------------------------------------------------

namespace test {
// copying throws; moving doesn't
struct throwing_copy {
    int value{};
    throwing_copy() = default;
    explicit throwing_copy(int v) : value(v) {}
    throwing_copy(const throwing_copy&) { throw std::runtime_error("throwing_copy"); }
    throwing_copy(throwing_copy&&) noexcept = default;
};
// moving may throw (according to its declaration) but never actually does
struct potentially_throwing_move {
    int value{};
    potentially_throwing_move() = default;
    explicit potentially_throwing_move(int v) : value(v) {}
    potentially_throwing_move(const potentially_throwing_move&) = default;
    potentially_throwing_move(potentially_throwing_move&& other) noexcept(false) : value(other.value) {}
};

template <typename T>
auto make_value() -> T {
    if constexpr (std::same_as<T, int>)
        return 17;
    else if constexpr (std::same_as<T, double>)
        return 2.5;
    else if constexpr (std::same_as<T, std::exception_ptr>)
        return std::make_exception_ptr(std::runtime_error("upstream"));
    else if constexpr (std::same_as<T, std::string>)
        return "a string long enough to avoid the small string optimization";
    else
        return T(42);
}

inline auto describe(int v) -> std::string { return "int:" + std::to_string(v); }
inline auto describe(double v) -> std::string { return "double:" + std::to_string(v); }
inline auto describe(const std::string& v) -> std::string { return "string:" + v; }
inline auto describe(const throwing_copy& v) -> std::string { return "throwing_copy:" + std::to_string(v.value); }
inline auto describe(const potentially_throwing_move& v) -> std::string {
    return "potentially_throwing_move:" + std::to_string(v.value);
}
inline auto describe(const std::exception_ptr& ptr) -> std::string {
    try {
        std::rethrow_exception(ptr);
    } catch (const std::exception& ex) {
        return std::string("exception:") + ex.what();
    } catch (...) {
        return "exception:unknown";
    }
}
template <typename... T>
auto describe(const std::tuple<T...>& t) -> std::string {
    return std::apply([](const auto&... v) { return (std::string("tuple(") + ... + (describe(v) + ",")) + ")"; }, t);
}
template <typename... T>
auto describe(const std::variant<T...>& v) -> std::string {
    return std::visit([](const auto& a) { return "variant(" + describe(a) + ")"; }, v);
}
template <typename T>
auto describe(const std::optional<T>& opt) -> std::string {
    return opt ? "optional(" + describe(*opt) + ")" : std::string("nullopt");
}
#if defined(__cpp_lib_expected)
template <typename T, typename E>
auto describe(const std::expected<T, E>& exp) -> std::string {
    return exp ? "expected(" + describe(*exp) + ")" : "unexpected(" + describe(exp.error()) + ")";
}
template <typename E>
auto describe(const std::expected<void, E>& exp) -> std::string {
    return exp ? std::string("expected()") : "unexpected(" + describe(exp.error()) + ")";
}
#endif
template <typename T>
auto category(T&&) -> std::string {
    return std::is_lvalue_reference_v<T> ? (std::is_const_v<std::remove_reference_t<T>> ? "const&" : "&") : "&&";
}

// Receiver logging the completion it receives (including the value categories).
template <typename Env = ::beman::execution::env<>>
struct logging_receiver {
    using receiver_concept = ::beman::execution::receiver_tag;
    std::string* log;
    Env          env{};

    template <typename... Args>
    auto set_value(Args&&... args) && noexcept -> void {
        *this->log =
            (std::string("set_value(") + ... + (describe(args) + category(std::forward<Args>(args)) + ",")) + ")";
    }
    template <typename Error>
    auto set_error(Error&& error) && noexcept -> void {
        *this->log = "set_error(" + describe(error) + category(std::forward<Error>(error)) + ")";
    }
    auto set_stopped() && noexcept -> void { *this->log = "set_stopped()"; }
    auto get_env() const noexcept -> Env { return this->env; }
};

template <typename Sig>
struct choice_completion;
template <typename Tag, typename... Args>
struct choice_completion<Tag(Args...)> {
    template <typename Receiver>
    static auto complete(Receiver& receiver) -> void {
        std::tuple<std::decay_t<Args>...> args{make_value<std::decay_t<Args>>()...};
        std::apply([&](auto&... a) { Tag()(std::move(receiver), static_cast<Args&&>(a)...); }, args);
    }
};

// Sender completing with the completion Sigs...[which].
template <typename... Sigs>
struct choice_sender {
    using sender_concept = ::beman::execution::sender_tag;
    std::size_t which{};

    template <typename, typename...>
    static consteval auto get_completion_signatures() -> ::beman::execution::completion_signatures<Sigs...> {
        return {};
    }

    template <typename Receiver>
    struct state {
        using operation_state_concept = ::beman::execution::operation_state_tag;
        std::size_t which;
        Receiver    receiver;
        auto        start() & noexcept -> void {
            std::size_t index{};
            (void)((index++ == this->which ? (choice_completion<Sigs>::complete(this->receiver), true) : false) ||
                   ...);
        }
    };
    template <typename Receiver>
    auto connect(Receiver receiver) && noexcept -> state<Receiver> {
        return {this->which, std::move(receiver)};
    }
};

template <typename Sender, typename Env = ::beman::execution::env<>>
auto run_logged(Sender&& sender, Env env = {}) -> std::string {
    std::string log;
    auto        op{::beman::execution::connect(std::forward<Sender>(sender), logging_receiver<Env>{&log, env})};
    ::beman::execution::start(op);
    return log;
}

// Check that existing(child) and ext(child) have the same completion
// signatures and complete identically for each completion of child.
template <typename... Sigs>
auto check_equivalent(auto existing, auto ext) -> void {
    using child = choice_sender<Sigs...>;
    using existing_sigs =
        ::beman::execution::completion_signatures_of_t<decltype(existing(child{})), ::beman::execution::env<>>;
    using ext_sigs = ::beman::execution::completion_signatures_of_t<decltype(ext(child{})), ::beman::execution::env<>>;
    static_assert(std::same_as<existing_sigs, ext_sigs>);
    for (std::size_t which{}; which != sizeof...(Sigs); ++which) {
        auto existing_log{run_logged(existing(child{which}))};
        auto ext_log{run_logged(ext(child{which}))};
        if (existing_log != ext_log) {
#ifndef _MSC_VER
            std::cerr << "completion " << which << ": existing=" << existing_log << " ext=" << ext_log << "\n";
#endif
        }
        ASSERT(existing_log == ext_log);
    }
}
} // namespace test

// ----------------------------------------------------------------------------

#endif
