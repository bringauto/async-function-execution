#pragma once

#include <nlohmann/json.hpp>

#include <chrono>
#include <unordered_map>



namespace bringauto::async_function_execution::structures {

/**
 * @brief Configuration for individual functions in the AsyncFunctionExecutor.
 * timeout: Optional timeout duration for the specific function. If not set, the defaultTimeout from Settings is used.
 */
struct FunctionConfig {
	std::chrono::nanoseconds timeout = std::chrono::nanoseconds(0);
};

/**
 * @brief Configuration settings for the AsyncFunctionExecutor.
 * isProducer: If true, the instance acts as a producer (sending requests).
 * If false, it acts as a consumer (receiving requests and sending responses).
 * defaultTimeout: Default timeout duration for function calls.
 * functionConfigs: Optional per-function configurations, keyed by FunctionId.
 */
struct Settings {
	const bool isProducer = true;
	const std::chrono::nanoseconds defaultTimeout = std::chrono::nanoseconds(0);
	std::unordered_map<uint8_t, FunctionConfig> functionConfigs;

	Settings(bool isProducer, std::chrono::nanoseconds defaultTimeout, std::string_view funcConfs = "")
			: isProducer(isProducer), defaultTimeout(defaultTimeout) {
		if (funcConfs.empty()) {
			return;
		}

		const auto configs = nlohmann::json::parse(funcConfs, nullptr, false);
		for(const auto& [key, value] : configs.items()) {
			try {
				uint8_t funcId = static_cast<uint8_t>(std::stoi(key));
				FunctionConfig funcConfig {
					.timeout = std::chrono::nanoseconds(value["timeout"].get<int64_t>())
				};
				functionConfigs[funcId] = funcConfig;
			} catch (const std::exception &e) {
				std::cerr << "Error parsing function configuration for key " << key << ": " << e.what() << std::endl;
			}
		}
	}
};

}
