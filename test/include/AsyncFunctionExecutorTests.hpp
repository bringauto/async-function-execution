#pragma once

#include <bringauto/async_function_execution/AsyncFunctionExecutor.hpp>
#include <MockClient.hpp>

#include <gtest/gtest.h>



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

baafe::FunctionDefinition FunctionAdd {
	baafe::FunctionId { 1 },
	baafe::Return { int {} },
	baafe::Arguments { int {}, int {}, int {} }
};

baafe::FunctionDefinition FunctionMultiply {
	baafe::FunctionId { 2 },
	baafe::Return { int {} },
	baafe::Arguments { int {}, int {}, int {} }
};

baafe::FunctionDefinition FunctionReturnSame {
	baafe::FunctionId { 3 },
	baafe::Return { int {} },
	baafe::Arguments { int {} }
};

baafe::FunctionDefinition FunctionReturnSameString {
	baafe::FunctionId { 4 },
	baafe::Return { SerializableString {} },
	baafe::Arguments { SerializableString {} }
};

baafe::FunctionDefinition FunctionNoArgs {
	baafe::FunctionId { 5 },
	baafe::Return { int {} },
	baafe::Arguments { }
};

baafe::FunctionDefinition FunctionWait {
	baafe::FunctionId { 6 },
	baafe::Return<void> { },
	baafe::Arguments { }
};

baafe::AsyncFunctionExecutor executorProducer {
	baafe::Config {
		.isProducer = true,
		.defaultTimeout = std::chrono::seconds(1),
		.functionConfigurations = baafe::structures::FunctionConfigs { {
			{ FunctionAdd.id.value,              { std::chrono::nanoseconds(1000000) } },
			{ FunctionMultiply.id.value,         { std::chrono::nanoseconds(2000000) } },
			{ FunctionReturnSame.id.value,       { std::chrono::nanoseconds(3000000) } },
			{ FunctionReturnSameString.id.value, { std::chrono::nanoseconds(4000000) } },
			{ FunctionNoArgs.id.value,           { std::chrono::nanoseconds(5000000) } },
			{ FunctionWait.id.value,             { std::chrono::nanoseconds(6000000) } }
		} }
	},
	baafe::FunctionList {
		FunctionAdd,
		FunctionMultiply,
		FunctionReturnSame,
		FunctionReturnSameString,
		FunctionNoArgs,
		FunctionWait
	},
	std::make_unique<MockClient>()
};

baafe::AsyncFunctionExecutor executorConsumer {
	baafe::Config {
		.isProducer = false
	},
	baafe::FunctionList {
		FunctionAdd,
		FunctionMultiply,
		FunctionReturnSame,
		FunctionReturnSameString,
		FunctionNoArgs
	},
	std::make_unique<MockClient>()
};


class AsyncFunctionExecutorTests : public ::testing::Test {
protected:
	static void SetUpTestSuite() {
		executorProducer.connect();
		executorConsumer.connect();
	}

	void SetUp() override {}
	void TearDown() override {}
};
