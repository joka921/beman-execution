module;
// src/beman/execution/ext_transform_completion_channel.cppm          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/execution/detail/ext_transform_completion_channel.hpp>

export module beman.execution.detail.ext_transform_completion_channel;

namespace beman::execution {
export using beman::execution::ext_transform_completion_channel_t;
export using beman::execution::ext_transform_completion_channel;
export using beman::execution::ext_each_completion_signature_t;
export using beman::execution::ext_each_completion_signature;
} // namespace beman::execution

namespace beman::execution::detail {
export using beman::execution::detail::completion_signatures_transform;
export using beman::execution::detail::ext_transform_completion_channel_data;
export using beman::execution::detail::ext_transform_completion_channel_declared;
export using beman::execution::detail::ext_transform_completion_channel_sender;
export using beman::execution::detail::ext_transform_completion_channel_receiver;
export using beman::execution::detail::ext_single_value_argument_t;
} // namespace beman::execution::detail
