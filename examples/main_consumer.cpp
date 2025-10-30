#include <bringauto/async_function_execution/AsyncFunctionExecutor.hpp>
#include <bringauto/async_function_execution/AeronDriver.hpp>



using namespace bringauto::async_function_execution;

struct SerializableString final {
	std::string value {};
	SerializableString() = default;
	explicit SerializableString(std::string str) : value(std::move(str)) {}

	std::span<const uint8_t> serialize() const {
		return std::span {reinterpret_cast<const uint8_t *>(value.data()), value.size()};
	}
	void deserialize(std::span<const uint8_t> bytes) {
		value = std::string {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
	}
};

constexpr int ExampleFunc1Id = 1;
constexpr int ExampleFunc2Id = 2;
constexpr int ExampleFunc3Id = 3;

FunctionDefinition ExampleFunc1 {
	FunctionId { ExampleFunc1Id },
	Return { SerializableString {} },
	Arguments { int {}, SerializableString {}, float {} }
};

FunctionDefinition ExampleFunc2 {
	FunctionId { ExampleFunc2Id },
	Return { SerializableString {} },
	Arguments { int {}, SerializableString {} }
};

FunctionDefinition ExampleFunc3 {
	FunctionId { ExampleFunc3Id },
	Return { SerializableString {} },
	Arguments { int {} }
};


int main() {
	AsyncFunctionExecutor executor {
		Config {
			.isProducer = false,
		},
		FunctionList { ExampleFunc1, ExampleFunc2, ExampleFunc3 },
	};

	if (executor.connect() != 0) {
		std::cerr << "Consumer: Failed to connect to executor" << std::endl;
		return 1;
	}

	while (true) {
		auto [funcId, argBytes] = executor.pollFunction();
		
		switch (funcId.value) {
			case ExampleFunc1Id: {
				auto [arg1, arg2, arg3] = executor.getFunctionArgs(ExampleFunc1, argBytes);
				std::cout << "Consumer: Received Function 1 call with args (" << arg1 << ", " << arg2.value << ", " << arg3 << ")." << std::endl;
				executor.sendReturnMessage(funcId, SerializableString{"Func 1 return value"});
				break;
			}
			case ExampleFunc2Id: {
				auto [arg1, arg2] = executor.getFunctionArgs(ExampleFunc2, argBytes);
				std::cout << "Consumer: Received Function 2 call with args (" << arg1 << ", " << arg2.value << ")." << std::endl;
				executor.sendReturnMessage(funcId, SerializableString{"Func 2 return value"});
				break;
			}
			case ExampleFunc3Id: {
				auto [arg1] = executor.getFunctionArgs(ExampleFunc3, argBytes);
				std::cout << "Consumer: Received Function 3 call with args (" << arg1 << ")" << std::endl;
				executor.sendReturnMessage(funcId, SerializableString{"Func 3 return value"});
				break;
			}
			default:
				std::cerr << "Consumer: Unknown function ID received: " << static_cast<int>(funcId.value) << std::endl;
				throw std::runtime_error("Unknown function ID");
		}
	}

	return 0;
}
