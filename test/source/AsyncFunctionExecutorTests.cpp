#include <AsyncFunctionExecutorTests.hpp>



/**
 * @brief Tests calling different functions and receiving correct return values.
 */
TEST_F(AsyncFunctionExecutorTests, CallDifferentFunctions) {
	auto result = executorProducer.callFunc(FunctionAdd, 1, 2, 3);
	ASSERT_EQ(result, 6);
	result = executorProducer.callFunc(FunctionMultiply, 2, 3, 4);
	ASSERT_EQ(result, 24);
	result = executorProducer.callFunc(FunctionReturnSame, 42);
	ASSERT_EQ(result, 42);
}


/**
 * @brief Tests calling a function with a serializable string argument and return value.
 */
TEST_F(AsyncFunctionExecutorTests, CallFunctionWithSerializableString) {
	auto result = executorProducer.callFunc(FunctionReturnSameString, SerializableString{"Hello, World!"});
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
	// Consumer using callFunc should throw
	ASSERT_THROW(executorConsumer.callFunc(FunctionAdd, 1, 2, 3), std::runtime_error);

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
