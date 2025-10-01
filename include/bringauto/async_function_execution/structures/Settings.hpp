#pragma once

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
 * @brief Wrapper for multiple FunctionConfig instances, keyed by FunctionId.
 * configs: Map of FunctionId to FunctionConfig.
 */
struct FunctionConfigs {
	std::unordered_map<uint8_t, FunctionConfig> configs;
	FunctionConfigs() = default;
	FunctionConfigs(std::unordered_map<uint8_t, FunctionConfig> configs) : configs(std::move(configs)) {};
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
	FunctionConfigs functionConfigs;

	Settings(bool isProducer, std::chrono::nanoseconds defaultTimeout,
			 const FunctionConfigs& functionConfigs = {})
			: isProducer(isProducer), defaultTimeout(defaultTimeout), functionConfigs(functionConfigs) {}
};

}
