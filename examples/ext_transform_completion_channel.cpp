// examples/ext_transform_completion_channel.cpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <version>
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#if defined(__cpp_lib_expected)
#include <expected>
#endif
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution;
#else
#include <beman/execution.hpp>
#endif

namespace ex = beman::execution;

// ----------------------------------------------------------------------------
// ext_transform_completion_channel(sndr, fun, sig_transform) reroutes completions
// of sndr: fun receives a receiver and the completion and decides which
// completion to use. sig_transform maps the completion signatures of sndr to
// the signatures of the resulting sender. The example below defines an
// adaptor turning an int value completion into an error if it is negative.

namespace {
struct negative_as_error_fun {
    template <typename Receiver>
    auto operator()(Receiver& rcvr, ex::set_value_t, int value) const noexcept -> void {
        if (value < 0)
            ex::set_error(std::move(rcvr), std::string("negative value: ") + std::to_string(value));
        else
            ex::set_value(std::move(rcvr), value);
    }
};

// Transforms each set_value_t(int) signature; other signatures are retained.
struct negative_as_error_signatures {
    consteval auto operator()(ex::set_value_t (*)(int)) const {
        return ex::completion_signatures<ex::set_value_t(int), ex::set_error_t(std::string)>{};
    }
};

const auto negative_as_error{ex::ext_transform_completion_channel(
    negative_as_error_fun{}, ex::ext_each_completion_signature(negative_as_error_signatures{}))};

auto lookup(int key) -> std::optional<std::string> {
    if (key == 1)
        return "one";
    return std::nullopt;
}
} // namespace

auto main() -> int {
    // a custom adaptor built from ext_transform_completion_channel
    auto [v] = ex::sync_wait(ex::just(17) | negative_as_error).value();
    std::cout << "negative_as_error(17) -> " << v << "\n";
    try {
        ex::sync_wait(ex::just(-1) | negative_as_error);
    } catch (const std::string& error) {
        std::cout << "negative_as_error(-1) -> error: " << error << "\n";
    }

    // ext_nullopt_as_stopped: an empty optional becomes a stopped completion
    for (int key : {1, 2}) {
        auto result = ex::sync_wait(ex::just(key) | ex::then(lookup) | ex::ext_nullopt_as_stopped());
        std::cout << "ext_nullopt_as_stopped(lookup(" << key << ")) -> "
                  << (result ? std::get<0>(*result) : std::string("stopped")) << "\n";
    }

    // ext_nullopt_as_error: an empty optional becomes an error completion
    try {
        ex::sync_wait(ex::just(2) | ex::then(lookup) | ex::ext_nullopt_as_error(std::string("key not found")));
    } catch (const std::string& error) {
        std::cout << "ext_nullopt_as_error(lookup(2)) -> error: " << error << "\n";
    }

    // chains of adaptors built on ext_transform_completion_channel are merged into one sender
    auto chain = ex::just(1) | ex::ext_then([](int key) { return lookup(key); }) | ex::ext_nullopt_as_stopped() |
                 ex::ext_then([](std::string s) { return s + "!"; });
    std::cout << "merged chain -> " << std::get<0>(ex::sync_wait(std::move(chain)).value()) << "\n";

    // ext_nullopt_as_void: an empty optional becomes a value completion without arguments
    auto r = ex::sync_wait_with_variant(ex::just(2) | ex::then(lookup) | ex::ext_nullopt_as_void());
    std::cout << "ext_nullopt_as_void(lookup(2)) -> variant index " << r.value().index() << "\n";

#if defined(__cpp_lib_expected)
    // ext_unexpected_as_error: the error of an expected becomes an error completion
    try {
        ex::sync_wait(ex::just(std::expected<int, std::string>(std::unexpected(std::string("unexpected")))) |
                      ex::ext_unexpected_as_error());
    } catch (const std::string& error) {
        std::cout << "ext_unexpected_as_error -> error: " << error << "\n";
    }

    // ext_error_as_unexpected: a value or an error becomes a value completion with an expected
    auto checked = [](int value) {
        if (value < 0)
            throw std::string("negative value");
        return value;
    };
    for (int value : {17, -1}) {
        auto [exp] = ex::sync_wait(ex::just(value) | ex::then(checked) | ex::ext_error_as_unexpected()).value();
        std::cout << "ext_error_as_unexpected(checked(" << value << ")) -> "
                  << (exp ? "value " + std::to_string(*exp) : std::string("error (exception_ptr)")) << "\n";
    }
#endif
}
