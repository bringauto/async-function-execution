#pragma once

#include <bringauto/async_function_execution/clients/ClientInterface.hpp>

#include <gtest/gtest.h>

#include <cstring>
#include <iostream>



class MockClient final : public bringauto::async_function_execution::clients::ClientInterface {
public:
	MockClient() = default;
	~MockClient() = default;

	int connect(const std::vector<uint32_t>& subscriptionIds, const std::vector<uint32_t>& publicationIds) override {
		(void)subscriptionIds; (void)publicationIds;
		return 0;
	}

	int sendMessage(const uint32_t channelId, std::span<const uint8_t> &messageBytes) override {
		if (channelId > 1000) {
			// Validate that this is a return message
			if (messageBytes.size() != 3 + sizeof(int)) {
				std::cerr << "Invalid return message size: " << messageBytes.size() << std::endl;
				return -1; // Error: Invalid return message size
			}

			if (messageBytes[0] != static_cast<uint8_t>(channelId - 1000) || messageBytes[1] != sizeof(int)) {
				std::cerr << "Invalid return message format." << std::endl;
				return -2; // Error: Invalid return message format
			}

			int returnValue;
			std::memcpy(&returnValue, messageBytes.data() + 3, sizeof(int));
			
			if (returnValue != 42) {
				std::cerr << "Invalid return value: " << returnValue << std::endl;
				return -3; // Error: Invalid return value
			}
			return 0;
		}

		const uint8_t funcId = messageBytes[0];

		if (funcId == 4) { // FunctionReturnSameString
			auto stringArgs = deserializeStringRequest(messageBytes);
			if (stringArgs.size() == 1) {
				serializeStringResponse(funcId, stringArgs[0]);
			}
			return 0;
		}

		if (funcId == 6) { // FunctionWait does not expect a response
			messageBuffer_.clear();
			return 0;
		}

		const auto args = deserializeIntRequest(messageBytes);
		switch (funcId) {
			case 1: // FunctionAdd
				if (args.size() == 3) {
					const int sum = args[0] + args[1] + args[2];
					serializeIntResponse(funcId, sum);
				}
				break;
			case 2: // FunctionMultiply
				if (args.size() == 3) {
					const int product = args[0] * args[1] * args[2];
					serializeIntResponse(funcId, product);
				}
				break;
			case 3: // FunctionReturnSame
				if (args.size() == 1) {
					serializeIntResponse(funcId, args[0]);
				}
				break;
			default:
				break;
		}
		return 0;
	};

	std::span<const uint8_t> waitForMessage(const uint32_t channelId, const std::chrono::nanoseconds timeout) override {
		// Test if the timeout is correctly set for each function
		EXPECT_EQ((channelId - 1000) * 1000000, timeout.count());

		if (channelId == 6) {
			// Simulate a long wait for FunctionWait
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			messageBuffer_.clear(); // No message to return
			return {};
		}

		if (messageBuffer_.empty()) {
			return {};
		}
		return {messageBuffer_.data(), messageBuffer_.size()};
	}

	/// Will always return a message for FunctionAdd with arguments (10, 20, 30)
	std::span<const uint8_t> waitForAnyMessage() override {
		messageBuffer_.clear();
		messageBuffer_.reserve(2 + 3 * (2 + sizeof(int))); // Function ID + Arg count + 3 args (size + data)
		messageBuffer_.push_back(1); // Function ID
		messageBuffer_.push_back(3); // Argument count
		int arg1 = 10;
		int arg2 = 20;
		int arg3 = 30;
		messageBuffer_.push_back(sizeof(int) & 0xFF);
		messageBuffer_.push_back(sizeof(int) >> 8 & 0xFF);
		messageBuffer_.insert(messageBuffer_.end(), reinterpret_cast<uint8_t*>(&arg1), reinterpret_cast<uint8_t*>(&arg1) + sizeof(int));
		messageBuffer_.push_back(sizeof(int) & 0xFF);
		messageBuffer_.push_back(sizeof(int) >> 8 & 0xFF);
		messageBuffer_.insert(messageBuffer_.end(), reinterpret_cast<uint8_t*>(&arg2), reinterpret_cast<uint8_t*>(&arg2) + sizeof(int));
		messageBuffer_.push_back(sizeof(int) & 0xFF);
		messageBuffer_.push_back(sizeof(int) >> 8 & 0xFF);
		messageBuffer_.insert(messageBuffer_.end(), reinterpret_cast<uint8_t*>(&arg3), reinterpret_cast<uint8_t*>(&arg3) + sizeof(int));
		return {messageBuffer_.data(), messageBuffer_.size()};
	}

private:
	/// Deserializes a request message into function ID and argument values the same way that AsyncFunctionExecutor does.
	std::vector<int> deserializeIntRequest(const std::span<const uint8_t> &bytes) {
		size_t pos = 1;
		const uint8_t argCount = bytes[pos++];
		std::vector<int> args;

		for (uint8_t i = 0; i < argCount; ++i) {
			const uint16_t argSize = bytes[pos] | (static_cast<uint16_t>(bytes[pos + 1]) << 8);
			pos += 2;
			int argValue;
			std::memcpy(&argValue, bytes.data() + pos, sizeof(int));
			args.push_back(argValue);
			pos += argSize;
		}
		return args;
	}

	/// Deserializes a request message with string arguments.
	std::vector<std::string> deserializeStringRequest(const std::span<const uint8_t> &bytes) {
		size_t pos = 1;
		const uint8_t argCount = bytes[pos++];
		std::vector<std::string> args;

		for (uint8_t i = 0; i < argCount; ++i) {
			const uint16_t argSize = bytes[pos] | (static_cast<uint16_t>(bytes[pos + 1]) << 8);
			pos += 2;
			std::string argValue(reinterpret_cast<const char*>(bytes.data() + pos), argSize);
			args.push_back(argValue);
			pos += argSize;
		}
		return args;
	}

	/// Serializes a response message the same way that AsyncFunctionExecutor does.
	void serializeIntResponse(const uint8_t funcId, const int returnValue) {
		std::vector<uint8_t> buffer;
		buffer.push_back(funcId);
		buffer.push_back(sizeof(int));
		buffer.resize(3 + sizeof(int));
		std::memcpy(buffer.data() + 3, &returnValue, sizeof(int));
		messageBuffer_ = buffer;
	}

	/// Serializes a response message with a string return value.
	void serializeStringResponse(const uint8_t funcId, const std::string &data) {
		std::vector<uint8_t> buffer;
		buffer.push_back(funcId);
		if (data.size() > 65535) {
			throw std::invalid_argument("Data too large to serialize in MockClient");
		}
		const auto size = static_cast<uint16_t>(data.size());
		buffer.push_back(static_cast<uint8_t>(size & 0xFF));
		buffer.push_back(static_cast<uint8_t>(size >> 8 & 0xFF));
		buffer.insert(buffer.end(), data.begin(), data.end());
		messageBuffer_ = buffer;
	}


	std::vector<uint8_t> messageBuffer_;
};
