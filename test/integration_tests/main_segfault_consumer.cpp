#include <bringauto/async_function_execution/AsyncFunctionExecutor.hpp>
#include <bringauto/async_function_execution/AeronDriver.hpp>

#include <iostream>


namespace baafe = bringauto::async_function_execution;

struct SerializableString final {
	std::string value {};
	SerializableString() = default;
	SerializableString(std::string str) : value(std::move(str)) {}

	std::span<const uint8_t> serialize() const {
		return std::span {reinterpret_cast<const uint8_t *>(value.data()), value.size()};
	}
	void deserialize(std::span<const uint8_t> bytes) {
		value = std::string {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
	}
};

baafe::FunctionDefinition Function {
	baafe::FunctionId { 1 },
	baafe::Return { SerializableString {} },
	baafe::Arguments { SerializableString {} }
};

baafe::AsyncFunctionExecutor executorConsumer {
	baafe::Config {
		.isProducer = false
	},
	baafe::FunctionList {
		Function
	}
};


int main() {
	executorConsumer.connect();
	std::cout << "Consumer connected." << std::endl;

	while (true) {
		auto [funcId, argBytes] = executorConsumer.pollFunction();
		if (funcId.value != 1) {
			std::cerr << "Consumer: Unknown function ID received: " << static_cast<int>(funcId.value) << std::endl;
			throw std::runtime_error("Unknown function ID");
		}
		auto [arg] = executorConsumer.getFunctionArgs(Function, argBytes);
		std::cout << "Consumer: Received Function call with args (" << arg.value << "). Sending back result: " << arg.value << std::endl;
		executorConsumer.sendReturnMessage(funcId, arg);

		// Force segfault to test if producer is affected
		int *p = nullptr;
		*p = 42;

		return 0;
	}

	return 0;
}
