#pragma once

#include <string_view>



namespace bringauto::async_function_execution {

constexpr int POLL_FRAGMENTS_LIMIT = 10;
constexpr int MESSAGE_RETURN_CHANNEL_OFFSET = 1000;
constexpr int MAX_ARGUMENT_SIZE = 65535;

constexpr std::string_view DEFAULT_AERON_CONNECTION = "aeron:ipc";
	
}
