#pragma once

#include <span>
#include <vector>
#include <chrono>



namespace bringauto::async_function_execution::clients {

class ClientInterface {
public:
	ClientInterface() = default;
	virtual ~ClientInterface() = default;

	/**
	 * @brief Sets up the client communication. Must be called before any other method.
	 * 
	 * @param subscriptionIds A vector of subscription IDs to connect to.
	 * @param publicationIds A vector of publication IDs to connect to.
	 * @return Returns 0 on success, or a negative error code on failure.
	 */
	virtual int connect(const std::vector<uint32_t>& subscriptionIds, const std::vector<uint32_t>& publicationIds) = 0;

	/**
	 * @brief Sends a message to the specified channel ID.
	 * 
	 * @param channelId The channel ID to send the message to.
	 * @param messageBytes The bytes of the message to send.
	 * @return Returns 0 on success, or a negative error code on failure.
	 */
	virtual int sendMessage(uint32_t channelId, std::span<const uint8_t> &messageBytes) = 0;

	/**
	 * @brief Waits for a message from the specified channel ID.
	 * 
	 * @param channelId The channel ID to wait for a message from.
	 * @param timeout Maximum time to wait for a message before timing out.
	 * @return Bytes of the last message received. Returns an empty span on timeout or error.
	 */
	virtual std::span<const uint8_t> waitForMessage(uint32_t channelId, std::chrono::nanoseconds timeout) = 0;

	/**
	 * @brief Waits for any message from all channels.
	 * 
	 * @return Bytes of the last message received from any channel. Returns an empty span on timeout or error.
	 */
	virtual std::span<const uint8_t> waitForAnyMessage() = 0;
};

}
