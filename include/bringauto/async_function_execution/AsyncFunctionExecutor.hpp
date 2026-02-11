#pragma once

#include <bringauto/async_function_execution/clients/AeronClient.hpp>
#include <bringauto/async_function_execution/TimeoutIdleStrategy.hpp>
#include <bringauto/async_function_execution/structures/Settings.hpp>

#include <utility>
#include <stdexcept>
#include <iostream>
#include <expected>
#include <unordered_map>



namespace bringauto::async_function_execution {

/**
 * @brief Configuration structure for the AsyncFunctionExecutor.
 * Contains settings for producer/consumer mode and default timeout.
 */
struct Config {
	bool isProducer = true;
	std::chrono::nanoseconds defaultTimeout = std::chrono::nanoseconds(0);
	structures::FunctionConfigs functionConfigurations {};
};


/**
 * @brief Unique identifier for a function in the AsyncFunctionExecutor.
 * value: The function ID value. Supported range is 0-255.
 */
struct FunctionId {
	const uint8_t value;
};


/**
 * @brief Concept to check if a type has methods serialize() and deserialize().
 */
template <typename T>
concept HasSerialize = requires(const T& t) {
	{ t.serialize() } -> std::convertible_to<std::span<const uint8_t>>;
};


/**
 * @brief Structure representing the return type of function.
 * value: The return type value.
 */
template<typename T>
struct Return {
	const T value;
	constexpr explicit Return(T &&val) : value(std::forward<T>(val)) {}
};


/**
 * @brief Specialization of Return for void type.
 */
template<>
struct Return<void> {
	constexpr Return() noexcept = default;
};


/**
 * @brief Structure representing the argument types of a function.
 * values: A tuple holding the types of the function arguments.
 */
template<typename... Args>
struct Arguments {
	const std::tuple<Args...> values;
	constexpr explicit Arguments(Args &&...args) : values{std::forward<Args>(args)...} {}
};


/**
 * @brief Definition of a function that can be called or responded to via AsyncFunctionExecutor.
 * id: Unique identifier for the function.
 * returnType: The return type of the function.
 * argumentTypes: The argument types of the function.
 */
template<typename Ret, typename... Args>
struct FunctionDefinition {
	const FunctionId id;
	const Return<Ret> returnType;
	const Arguments<Args...> argumentTypes;
};


/**
 * @brief Helper type trait to identify FunctionDefinition types.
 */
template<typename T>
struct is_function_definition : std::false_type {};


/**
 * @brief Specialization of is_function_definition for FunctionDefinition types.
 */
template<typename Ret, typename... Args>
struct is_function_definition<FunctionDefinition<Ret, Args...>> : std::true_type {};


/**
 * @brief Concept to ensure a type is a FunctionDefinition.
 */
template<typename T>
concept IsFunctionDefinition = requires { typename std::decay_t<T>; } &&
	is_function_definition<std::decay_t<T>>::value;


/**
 * @brief Helper structure used to store function definitions.
 * functions: A tuple holding all the function definitions.
 */
template<IsFunctionDefinition... Funcs>
struct FunctionList {
	const std::tuple<Funcs...> functions;
	explicit FunctionList(Funcs... funcs) : functions(std::move(funcs)...) {}
};


/**
 * @brief Enum class representing possible error states during function calls.
 */
enum class CallError {
	InvalidExecutorType,   // Called a producer-only function in consumer mode or vice versa
	FunctionIdNotDefined,  // Function ID is not defined in the FunctionList
	ArgumentCountMismatch, // Number of expected arguments does not match
	TimeoutOrNoResponse,   // No response received within the timeout
	FunctionIdMismatch,    // Function ID does not match
	FunctionCallInProgress // Function call is already in progress
};


/**
 * @brief This class provides a high-level interface for async communication, allowing function calls over shared memory
 * with serialization and deserialization.
 */
template<typename... Funcs>
class AsyncFunctionExecutor {
public:
	/**
	 * @brief Constructs an AsyncFunctionExecutor instance. The connect() method must be called before usage.
	 * 
	 * @param config Configuration for the async function executor.
	 * @param functions A list of function definitions that the client can call or respond to.
	 * @param client Optional custom client implementing ClientInterface. If not provided, a default AeronClient is used.
	 */
	AsyncFunctionExecutor(const Config& config,
						  const FunctionList<Funcs...> &functions,
						  std::unique_ptr<clients::ClientInterface> client = nullptr)
			: client_(nullptr), settings_(config.isProducer, config.defaultTimeout, config.functionConfigurations), functions_(functions) {
		// Default client if none is provided
		if (client) {
			client_ = std::move(client);
		} else {
			client_ = std::make_unique<clients::AeronClient<TimeoutIdleStrategy>>(
				DEFAULT_AERON_CONNECTION, TimeoutIdleStrategy(settings_.defaultTimeout)
			);
		}

		for (const auto &funcId: settings_.functionConfigs.getFunctionIds()) {
			if (!isFunctionDefined(FunctionId{funcId})) {
				throw std::runtime_error("Warning: Function ID " + std::to_string(funcId) + " in configuration is not defined in FunctionList.");
			}
		}
	};

	~AsyncFunctionExecutor() = default;


	/**
	 * @brief Connects the client to the media driver and sets up communication channels.
	 * Needs to be called before any function calls or polling.
	 * @param channelOffset Optional offset to add to all function channel IDs. Default is 0.
	 * Use this when multiple executors are used in the same process to avoid channel ID conflicts.
	 * 
	 * @return Returns 0 on success, or a negative error code on failure.
	 */
	int connect(const uint32_t channelOffset = 0) {
		if (channelOffset > (UINT32_MAX / (MESSAGE_RETURN_CHANNEL_OFFSET * 10))) {
			std::cerr << "Channel offset too large" << std::endl;
			return -1; // Error: Channel offset too large
		}
		// multiplied by 10 because channel offset needs to be one order of magnitude larger than the return channel offset to avoid conflicts
		channelOffset_ = channelOffset * (MESSAGE_RETURN_CHANNEL_OFFSET * 10);

		std::vector<uint32_t> toProducer;
		std::vector<uint32_t> fromProducer;

		std::apply([&](const auto&... funcDefs) {
			(toProducer.push_back(funcDefs.id.value + channelOffset_ + MESSAGE_RETURN_CHANNEL_OFFSET), ...);
			(fromProducer.push_back(funcDefs.id.value + channelOffset_), ...);
			(callInProgress_.emplace(funcDefs.id.value, false), ...);
		}, functions_.functions);

		if (settings_.isProducer) {
			return client_->connect(toProducer, fromProducer);
		}
		return client_->connect(fromProducer, toProducer);
	}


	/**
	 * @brief Calls a function defined in the FunctionList, sending arguments and waiting for a response.
	 * Can only be used in producer mode.
	 * 
	 * @param function The function definition of which function to call.
	 * @param args The arguments to pass to the function.
	 * @return The return value of the function. If the return type contains some byte buffer,
	 * the memory is valid until the next call to callFunc(). On error, returns a CallError enum value.
	 */
	template <typename Ret, typename... FArgs, typename... CallArgs>
	auto callFunc(const FunctionDefinition<Ret, FArgs...> &function, CallArgs&&... args) -> std::expected<Ret, CallError> {
		if (!settings_.isProducer) {
			return std::unexpected(CallError::InvalidExecutorType);
		}
		if (sizeof...(CallArgs) < sizeof...(FArgs)) {
			return std::unexpected(CallError::ArgumentCountMismatch);
		}
		if (!isFunctionDefined(function.id)) {
			return std::unexpected(CallError::FunctionIdNotDefined);
		}
		if (callInProgress_[function.id.value].load()) {
			return std::unexpected(CallError::FunctionCallInProgress);
		}
		
		callInProgress_[function.id.value] = true;
		auto messageBytes = serializeArgs(function.id, args...);
		client_->sendMessage(function.id.value + channelOffset_, messageBytes);

		auto timeout = settings_.functionConfigs.getConfig(function.id.value).timeout;
		auto responseBytes = client_->waitForMessage(function.id.value + channelOffset_ + MESSAGE_RETURN_CHANNEL_OFFSET,
													 timeout == std::chrono::nanoseconds(0) ? settings_.defaultTimeout : timeout);

		if (responseBytes.empty()) {
			callInProgress_[function.id.value] = false;
			return std::unexpected(CallError::TimeoutOrNoResponse);
		}

		auto response = deserializeReturn<Ret>(function.id, responseBytes);
		callInProgress_[function.id.value] = false;
		return response;
	}


	/**
	 * @brief Polls for incoming function calls and returns the function ID and arguments represented as bytes.
	 * These bytes can then be deserialized using getFunctionArgs(). Can only be used in consumer mode.
	 * 
	 * @return A tuple containing the FunctionId and a span of argument bytes. Returns an empty tuple on error.
	 */
	std::tuple<FunctionId, std::span<const uint8_t>> pollFunction() {
		if (settings_.isProducer) {
			std::cerr << "Cannot start polling in producer mode." << std::endl;
			return std::make_tuple(FunctionId{}, std::span<const uint8_t>{}); // Error: Cannot start polling in producer mode
		}

		const auto requestBytes = client_->waitForAnyMessage();
		if (requestBytes.empty()) {
			return std::make_tuple(FunctionId{}, std::span<const uint8_t>{}); // Error: No message received or timeout
		}

		auto [funcId, argBytes] = deserializeRequest(requestBytes);
		return std::make_tuple(funcId, argBytes);
	}


	/**
	 * @brief Deserializes function arguments from a byte span into a tuple of argument values.
	 * Can only be used in consumer mode. Throws on error.
	 * 
	 * @param function The function definition corresponding to the arguments.
	 * @param argBytes The byte span containing the serialized arguments.
	 * @return A tuple containing the deserialized argument values.
	 */
	template<typename Ret, typename... Args>
	auto getFunctionArgs(const FunctionDefinition<Ret, Args...> &function, const std::span<const uint8_t> &argBytes) {
		if (settings_.isProducer) {
			throw std::runtime_error("Cannot get function arguments in producer mode");
		}

		if (!isFunctionDefined(function.id)) {
			throw std::runtime_error("Function ID not defined");
		}

		if (argBytes.empty()) {
			throw std::invalid_argument("Not enough data to read argument count");
		}

		size_t pos = 0;
		uint8_t argCount = argBytes[pos++];

		if (argCount != sizeof...(Args)) {
			throw std::invalid_argument("Argument count mismatch");
		}

		std::tuple<Args...> args;
		auto extractArg = [&](auto &arg) {
			if (pos >= argBytes.size()) throw std::invalid_argument("Unexpected end of data while reading argument size");
			const uint16_t len = argBytes[pos] | (static_cast<uint16_t>(argBytes[pos + 1]) << 8);
			pos += 2;
			if (pos + len > argBytes.size()) throw std::invalid_argument("Unexpected end of data while reading argument content");
			
			if constexpr (HasSerialize<decltype(arg)>) {
				arg.deserialize(std::span {argBytes.data() + pos, len});
			} else {
				std::memcpy(&arg, argBytes.data() + pos, len);
			}

			pos += len;
		};

		std::apply([&](auto &... tupleArgs) {
			(extractArg(tupleArgs), ...);
		}, args);

		return args;
	}


	/**
	 * @brief Sends a return message for a previously polled function call.
	 * Can only be used in consumer mode.
	 * 
	 * @param functionId The FunctionId of the function call to respond to.
	 * @param returnValue The return value to send back.
	 * @return 0 on success, or a negative error code on failure.
	 */
	template<typename T>
	int sendReturnMessage(const FunctionId &functionId, const T &returnValue) {
		if (!isFunctionDefined(functionId)) {
			std::cerr << "Function ID not defined." << std::endl;
			return -1; // Error: Function ID not defined
		}

		if (settings_.isProducer) {
			std::cerr << "Cannot send return message in producer mode." << std::endl;
			return -1; // Error: Cannot send return message in producer mode
		}

		auto messageBytes = serializeReturn(functionId, returnValue);
		return client_->sendMessage(functionId.value + channelOffset_ + MESSAGE_RETURN_CHANNEL_OFFSET, messageBytes);
	}

private:
	bool isFunctionDefined(const FunctionId &funcId) {
		bool found = false;
		std::apply([&](const auto&... funcDefs) {
			((funcDefs.id.value == funcId.value ? found = true : false), ...);
		}, functions_.functions);
		return found;
	}

	template<typename... Args>
	std::span<const uint8_t> serializeArgs(const FunctionId &funcId, const Args&... args) {
		serializationBuffers_[funcId.value].clear();
		std::size_t totalSize = 2; // Function ID + Argument count
		((totalSize += 2 + sizeof(args)), ...); // Each argument: size bytes + data
		serializationBuffers_[funcId.value].reserve(totalSize);
		serializationBuffers_[funcId.value].push_back(funcId.value);
		serializationBuffers_[funcId.value].push_back(static_cast<uint8_t>(sizeof...(Args)));
		(appendArg(serializationBuffers_[funcId.value], args), ...);
		return {serializationBuffers_[funcId.value].data(), serializationBuffers_[funcId.value].size()};
	}


	template<typename T>
	std::span<const uint8_t> serializeReturn(const FunctionId &funcId, const T &returnValue) {
		serializationBuffers_[funcId.value].clear();
		const std::size_t totalSize = 3 + sizeof(returnValue);
		serializationBuffers_[funcId.value].reserve(totalSize);
		serializationBuffers_[funcId.value].push_back(funcId.value);
		appendArg(serializationBuffers_[funcId.value], returnValue);
		return {serializationBuffers_[funcId.value].data(), serializationBuffers_[funcId.value].size()};
	}


	template<typename T>
	void appendArg(std::vector<uint8_t>& buffer, const T& arg) {
		if constexpr (HasSerialize<T>) {
			auto bytes = arg.serialize();
			if (bytes.size() > MAX_ARGUMENT_SIZE) {
				throw std::invalid_argument("Serialized data too large");
			}
			const auto size = static_cast<uint16_t>(bytes.size());
			buffer.push_back(static_cast<uint8_t>(size & 0xFF));
			buffer.push_back(static_cast<uint8_t>(size >> 8 & 0xFF));
			buffer.insert(buffer.end(), bytes.begin(), bytes.end());
		} else {
			static_assert(std::is_trivially_copyable_v<T>, "Argument type must be trivially copyable");
			const auto size = static_cast<uint16_t>(sizeof(arg));
			buffer.push_back(static_cast<uint8_t>(size & 0xFF));
			buffer.push_back(static_cast<uint8_t>(size >> 8 & 0xFF));
			buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&arg), reinterpret_cast<const uint8_t*>(&arg) + sizeof(T));
		}
	}


	template<typename T>
	auto deserializeReturn(const FunctionId &funcId, std::span<const uint8_t> bytes) -> std::expected<T, CallError> {
		if(funcId.value != bytes[0]) {
			return std::unexpected(CallError::FunctionIdMismatch);
		}

		if constexpr (std::is_same_v<T, void>) {
			return std::expected<void, CallError>{};
		} else {
			T value;
			if constexpr (HasSerialize<T>) {
				value.deserialize(std::span {bytes.data() + 3, bytes.size() - 3});
			} else {
				std::memcpy(&value, bytes.data() + 3, sizeof(T));
			}
			return value;
		}
	}


	std::tuple<FunctionId, std::span<const uint8_t>> deserializeRequest(std::span<const uint8_t> bytes) {
		if (bytes.empty()) {
			throw std::invalid_argument("Not enough data to deserialize request");
		}
		FunctionId funcId{ bytes[0] };
		std::span<const uint8_t> args(bytes.begin() + 1, bytes.end());
		return std::make_tuple(funcId, args);
	}


	/// Buffers used for serialization of messages.
	mutable std::unordered_map<uint8_t, std::vector<uint8_t>> serializationBuffers_;
	/// Client used for communication. Can be a custom implementation of ClientInterface.
	std::unique_ptr<clients::ClientInterface> client_;
	structures::Settings settings_;
	FunctionList<Funcs...> functions_;
	/// Map to track if a function call is in progress for a given function ID.
	std::unordered_map<uint8_t, std::atomic_bool> callInProgress_{};
	/// Channel ID offset to apply to all function channel IDs.
	uint32_t channelOffset_ {};
};

}
