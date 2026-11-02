module;
// src/beman/execution/ext_error_as_unexpected.cppm                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/execution/detail/ext_error_as_unexpected.hpp>

export module beman.execution.detail.ext_error_as_unexpected;

#if defined(__cpp_lib_expected)
namespace beman::execution {
export using beman::execution::ext_error_as_unexpected_t;
export using beman::execution::ext_error_as_unexpected;
} // namespace beman::execution
#endif
