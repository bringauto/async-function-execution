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
	FunctionConfigs() = default;
	explicit FunctionConfigs(std::unordered_map<uint8_t, FunctionConfig> configs) : configs(std::move(configs)) {};

	FunctionConfig getConfig(const uint8_t functionId) const {
		if (configs.find(functionId) != configs.end()) {
			return configs.at(functionId);
		}
		return FunctionConfig{};
	}

	void setConfig(const uint8_t functionId, const FunctionConfig& config) {
		configs[functionId] = config;
	}

	std::vector<uint8_t> getFunctionIds() const {
		std::vector<uint8_t> functionIds;
		for (const auto& [funcId, _] : configs) {
			functionIds.push_back(funcId);
		}
		return functionIds;
	}

	private:
		std::unordered_map<uint8_t, FunctionConfig> configs;
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

	Settings(const bool isProducer, const std::chrono::nanoseconds defaultTimeout,
			 const FunctionConfigs& functionConfigs = {})
			: isProducer(isProducer), defaultTimeout(defaultTimeout), functionConfigs(functionConfigs) {}
};

}
