#include <bringauto/async_function_execution/AsyncFunctionExecutor.hpp>
#include <bringauto/async_function_execution/AeronDriver.hpp>

#include <iostream>


namespace baafe = bringauto::async_function_execution;

baafe::FunctionDefinition FunctionAdd {
	baafe::FunctionId { 1 },
	baafe::Return { int {} },
	baafe::Arguments { int {}, int {}, int {} }
};

baafe::AsyncFunctionExecutor executorProducer {
	baafe::Config {
		.isProducer = true,
		.defaultTimeout = std::chrono::seconds(1)
	},
	baafe::FunctionList { std::tuple{
		FunctionAdd
	} }
};

baafe::AsyncFunctionExecutor executorConsumer {
	baafe::Config {
		.isProducer = false
	},
	baafe::FunctionList { std::tuple{
		FunctionAdd
	} }
};

void consumerLoop() {
	while (true) {
		auto [funcId, argBytes] = executorConsumer.pollFunction();
		if (funcId.value != 1) {
			std::cerr << "Consumer: Unknown function ID received: " << static_cast<int>(funcId.value) << std::endl;
			throw std::runtime_error("Unknown function ID");
		}
		auto [arg1, arg2, arg3] = executorConsumer.getFunctionArgs(FunctionAdd, argBytes);
		int result = arg1 + arg2 + arg3;
		std::cout << "Consumer: Received FunctionAdd call with args (" << arg1 << ", " << arg2 << ", " << arg3 << "). Sending back result: " << result << std::endl;
		executorConsumer.sendReturnMessage(funcId, result);
		return;
	}
}


int main() {
	baafe::AeronDriver driver;
	std::thread driverThread([&driver]() {
		driver.run();
	});

	// Wait a moment to ensure the driver is running
	std::this_thread::sleep_for(std::chrono::seconds(2));
	if (driver.isRunning()) {
		std::cout << "Aeron Driver is running." << std::endl;
	} else {
		std::cerr << "Aeron Driver failed to start." << std::endl;
		return -1;
	}

	executorProducer.connect();
	executorConsumer.connect();
	std::cout << "Both producer and consumer connected." << std::endl;

	// Start consumer loop in a separate thread
	std::thread consumerThread(consumerLoop);
	std::this_thread::sleep_for(std::chrono::seconds(1)); // Give consumer a moment to start

	// Producer calls FunctionAdd
	int sum = executorProducer.callFunc(FunctionAdd, 10, 20, 30);
	std::cout << "Producer: FunctionAdd(10, 20, 30) returned: " << sum << std::endl;
	if (sum != 60) {
		std::cerr << "Unexpected sum result!" << std::endl;
		return -1;
	}

	// Stop consumer loop
	consumerThread.join();
	std::cout << "Consumer loop has been stopped." << std::endl;

	// Stop the driver
	driver.stop();
	driverThread.join();
	std::cout << "Aeron Driver has been stopped." << std::endl;
	std::cout << "Test completed." << std::endl;
	return 0;
}