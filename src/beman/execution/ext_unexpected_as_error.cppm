module;
// src/beman/execution/ext_unexpected_as_error.cppm                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/execution/detail/ext_unexpected_as_error.hpp>

export module beman.execution.detail.ext_unexpected_as_error;

#if defined(__cpp_lib_expected)
namespace beman::execution {
export using beman::execution::ext_unexpected_as_error_t;
export using beman::execution::ext_unexpected_as_error;
} // namespace beman::execution
#endif
