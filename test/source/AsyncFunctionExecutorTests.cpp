#include <AsyncFunctionExecutorTests.hpp>



/**
 * @brief Tests calling different functions and receiving correct return values.
 */
TEST_F(AsyncFunctionExecutorTests, CallDifferentFunctions) {
	auto result = executorProducer.callFunc(FunctionAdd, 1, 2, 3).value();
	ASSERT_EQ(result, 6);
	result = executorProducer.callFunc(FunctionMultiply, 2, 3, 4).value();
	ASSERT_EQ(result, 24);
	result = executorProducer.callFunc(FunctionReturnSame, 42).value();
	ASSERT_EQ(result, 42);
}


/**
 * @brief Tests calling a function with a serializable string argument and return value.
 */
TEST_F(AsyncFunctionExecutorTests, CallFunctionWithSerializableString) {
	auto result = executorProducer.callFunc(FunctionReturnSameString, SerializableString{"Hello, World!"}).value();
	ASSERT_EQ(result.value, "Hello, World!");
}


/**
 * @brief Tests polling for a function call and deserializing the arguments.
 */
TEST_F(AsyncFunctionExecutorTests, PollFunction) {
	// Poll for FunctionAdd
	auto [funcId, argBytes] = executorConsumer.pollFunction();
	ASSERT_EQ(funcId.value, FunctionAdd.id.value);
	auto [arg1, arg2, arg3] = executorConsumer.getFunctionArgs(FunctionAdd, argBytes);
	ASSERT_EQ(arg1, 10);
	ASSERT_EQ(arg2, 20);
	ASSERT_EQ(arg3, 30);
}


/**
 * @brief Tests sending a return message. MockClient expects the return value to be 42.
 */
TEST_F(AsyncFunctionExecutorTests, SendReturnMessage) {
	int ret = executorConsumer.sendReturnMessage(FunctionAdd.id, 42);
	ASSERT_EQ(ret, 0);
}


/**
 * @brief Tests all types of invalid data for getFunctionArgs().
 */
TEST_F(AsyncFunctionExecutorTests, GetFunctionArgsInvalidData) {
	// Not enough data to read argument count
	std::vector<uint8_t> invalidData = {};
	ASSERT_THROW(executorConsumer.getFunctionArgs(FunctionAdd, invalidData), std::invalid_argument);

	// Argument count mismatch
	invalidData = { FunctionAdd.id.value, 2, 4, 1, 0, 0, 0, 4, 1, 0, 0, 0 }; // 2 ints with value 1
	ASSERT_THROW(executorConsumer.getFunctionArgs(FunctionAdd, invalidData), std::invalid_argument);

	// Unexpected end of data when reading argument size
	invalidData = { FunctionAdd.id.value, 3 };
	ASSERT_THROW(executorConsumer.getFunctionArgs(FunctionAdd, invalidData), std::invalid_argument);

	// Unexpected end of data when reading argument content
	invalidData = { FunctionAdd.id.value, 3, 4, 1 };
	ASSERT_THROW(executorConsumer.getFunctionArgs(FunctionAdd, invalidData), std::invalid_argument);
}


/**
 * @brief Tests checks for producer/consumer mode restrictions and error handling.
 */
TEST_F(AsyncFunctionExecutorTests, CallInvalidFunctionsProducerConsumer) {
	// Consumer using callFunc returns InvalidExecutorType error
	ASSERT_EQ(executorConsumer.callFunc(FunctionAdd, 1, 2, 3).error(), baafe::CallError::InvalidExecutorType);

	// Producer using pollFunction should return empty tuple
	auto [funcId, argBytes] = executorProducer.pollFunction();
	ASSERT_EQ(funcId.value, baafe::FunctionId{}.value);
	ASSERT_TRUE(argBytes.empty());

	// Producer using getFunctionArgs should throw
	ASSERT_THROW(executorProducer.getFunctionArgs(FunctionAdd, std::span<const uint8_t> {}), std::runtime_error);

	// Producer using sendReturnMessage should return error code
	int sendRet = executorProducer.sendReturnMessage(baafe::FunctionId {1}, 42);
	ASSERT_EQ(sendRet, -1);
}


/**
 * @brief Tests using an undefined function.
 */
TEST_F(AsyncFunctionExecutorTests, UndefinedFunction) {
	baafe::FunctionDefinition FunctionUndefined {
		baafe::FunctionId { 99 },
		baafe::Return { int {} },
		baafe::Arguments { int {}, int {} }
	};

	ASSERT_EQ(executorProducer.callFunc(FunctionUndefined, 1, 2).error(), baafe::CallError::FunctionIdNotDefined);
	ASSERT_THROW(executorConsumer.getFunctionArgs(FunctionUndefined, std::span<const uint8_t> {}), std::runtime_error);
	int ret = executorConsumer.sendReturnMessage(FunctionUndefined.id, 42);
	ASSERT_EQ(ret, -1);
}


/**
 * @brief Tests providing a configuration for an undefined function.
 */
TEST_F(AsyncFunctionExecutorTests, ConfigForUndefinedFunction) {
	// Function ID 99 is not defined in the FunctionList
	ASSERT_THROW(baafe::AsyncFunctionExecutor(
		baafe::Config {
			.isProducer = true,
			.functionConfigurations = baafe::structures::FunctionConfigs { {
				{ 99, { std::chrono::nanoseconds(1000000) } }
			} }
		},
		baafe::FunctionList {
			FunctionAdd,
			FunctionMultiply
		},
		std::make_unique<MockClient>()
	), std::runtime_error);
}


/**
 * @brief Tests calling a function with argument size over 2^16 bytes.
 */
TEST_F(AsyncFunctionExecutorTests, ArgumentTooLarge) {
	// Create a very large string argument
	std::string largeString(70000, 'A');
	ASSERT_THROW(executorProducer.callFunc(FunctionReturnSameString, SerializableString{largeString}), std::invalid_argument);
}


/**
 * @brief Tests the behavior when a function call is already in progress.
 */
TEST_F(AsyncFunctionExecutorTests, FunctionCallInProgress) {
	// Start a thread that calls FunctionWait, which does not expect a response
	std::thread waitThread([this]() {
		executorProducer.callFunc(FunctionWait);
	});
	// Give the thread a moment to start and set the callInProgress flag
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	// Now, calling FunctionWait again should return FunctionCallInProgress error
	EXPECT_EQ(executorProducer.callFunc(FunctionWait).error(), baafe::CallError::FunctionCallInProgress);
	waitThread.join();
}
