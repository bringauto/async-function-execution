#include <bringauto/async_function_execution/AsyncFunctionExecutor.hpp>


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

FunctionDefinition ExampleFunc1 {
	FunctionId { 1 },
	Return { SerializableString {} },
	Arguments { int {}, SerializableString {}, float {} }
};

FunctionDefinition ExampleFunc2 {
	FunctionId { 2 },
	Return { SerializableString {} },
	Arguments { int {}, SerializableString {} }
};

FunctionDefinition ExampleFunc3 {
	FunctionId { 3 },
	Return { SerializableString {} },
	Arguments { int {} }
};


int main() {
	AsyncFunctionExecutor executor {
		Config {
			.isProducer = true,
			.defaultTimeout = std::chrono::seconds(1)
		},
		FunctionList { ExampleFunc1, ExampleFunc2, ExampleFunc3 },
	};

	if (executor.connect() != 0) {
		std::cerr << "Producer: Failed to connect to executor" << std::endl;
		return 1;
	}

	auto result1 = executor.callFunc(ExampleFunc1, 42, "Hello", 3.14f).value();
	std::cout << result1.value << std::endl;
	auto result2 = executor.callFunc(ExampleFunc2, 100, "World").value();
	std::cout << result2.value << std::endl;
	auto result3 = executor.callFunc(ExampleFunc3, 123).value();
	std::cout << result3.value << std::endl;
	return 0;
}
