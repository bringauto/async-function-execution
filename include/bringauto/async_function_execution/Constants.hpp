#pragma once

#include <string_view>



namespace bringauto::async_function_execution {

/// Maximum number of fragments to process in a single poll operation.
constexpr int POLL_FRAGMENTS_LIMIT = 10;
/// Offset added to function ID to determine the return message channel ID.
constexpr unsigned int MESSAGE_RETURN_CHANNEL_OFFSET = 1000;
/// Maximum size for serialized function arguments.
constexpr unsigned int MAX_ARGUMENT_SIZE = 65535;

/// Default Aeron connection string for communication over shared memory.
constexpr std::string_view DEFAULT_AERON_CONNECTION = "aeron:ipc";

/// Timeout duration for establishing stream connections.
constexpr std::chrono::milliseconds STREAM_CONNECTION_TIMEOUT = std::chrono::milliseconds(5000);

}
