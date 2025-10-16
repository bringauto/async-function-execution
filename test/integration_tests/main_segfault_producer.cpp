#include <bringauto/async_function_execution/AsyncFunctionExecutor.hpp>
#include <bringauto/async_function_execution/AeronDriver.hpp>

#include <iostream>


namespace baafe = bringauto::async_function_execution;

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

baafe::FunctionDefinition Function {
	baafe::FunctionId { 1 },
	baafe::Return { SerializableString {} },
	baafe::Arguments { SerializableString {} }
};

baafe::AsyncFunctionExecutor executorProducer {
	baafe::Config {
		.isProducer = true,
		.defaultTimeout = std::chrono::seconds(1)
	},
	baafe::FunctionList {
		Function
	}
};

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

	if (executorProducer.connect() != 0) {
		std::cerr << "Producer: Failed to connect to executor" << std::endl;
		return 1;
	}

	std::cout << "Producer connected." << std::endl;
	std::cout << "Turn on the consumer and press Enter to continue..." << std::endl;
	std::cin.get();

	// Producer calls Function
	SerializableString ret = executorProducer.callFunc(Function, SerializableString{"Hello, World!"}).value();

	// Short delay while a segfault is forced in the consumer
	std::this_thread::sleep_for(std::chrono::seconds(1));
	
	std::cout << "Function returned: " << ret.value << std::endl;
	if (ret.value != "Hello, World!") {
		std::cerr << "Unexpected string result!" << std::endl;
		return -1;
	}

	// Stop the driver
	driver.stop();
	driverThread.join();
	std::cout << "Aeron Driver has been stopped." << std::endl;
	std::cout << "Test completed." << std::endl;
	return 0;
}
