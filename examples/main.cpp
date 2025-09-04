#include <bringauto/async_function_execution/AsyncFunctionExecutor.hpp>

#include <iostream>
#include <map>


using namespace bringauto::async_function_execution;

FunctionDefinition ExampleFunc1 {
	FunctionId { 1 },
	Return { std::string {} },
	Arguments { int {}, std::string {}, float {} }
};

FunctionDefinition ExampleFunc2 {
	FunctionId { 2 },
	Return { std::string {} },
	Arguments { int {}, std::string {} }
};

FunctionDefinition ExampleFunc3 {
	FunctionId { 3 },
	Return { std::string {} },
	Arguments { int {} }
};


int main() {
	AsyncFunctionExecutor executor {
		Config {
			.isProducer = true,
			.defaultTimeout = std::chrono::seconds(1)
		},
		FunctionList { std::tuple{ ExampleFunc1, ExampleFunc2, ExampleFunc3 } },
	};

	std::cout << executor.callFunc(ExampleFunc1, 42, "Hello", 3.14f) << std::endl;
	std::cout << executor.callFunc(ExampleFunc2, 100, "World") << std::endl;
	std::cout << executor.callFunc(ExampleFunc3, 123) << std::endl;
	return 0;
}
