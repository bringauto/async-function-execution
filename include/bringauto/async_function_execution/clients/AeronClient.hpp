#pragma once

#include <bringauto/async_function_execution/clients/ClientInterface.hpp>
#include <bringauto/async_function_execution/Constants.hpp>
#include <bringauto/async_function_execution/TimeoutIdleStrategy.hpp>

#include <Aeron.h>
#include <FragmentAssembler.h>
#include <ranges>
#include <concurrent/BackOffIdleStrategy.h>



namespace bringauto::async_function_execution::clients {

/**
 * This class is responsible for managing Aeron communication.
 */
template<typename IdleStrategy = aeron::BackoffIdleStrategy>
class AeronClient final : public ClientInterface {
public:
	struct MessageView {
		const uint8_t* data = nullptr;
		aeron::util::index_t length = 0;
	};


	/**
	 * @brief Constructs an AeronClient instance.
	 * 
	 * @param connection The Aeron connection string.
	 * @param idleStrategy An instance of the idle strategy to use for message handling.
	 * Initializes the Aeron context and sets up handlers.
	 */
	AeronClient(std::string_view connection, const IdleStrategy &idleStrategy) {
		aeronContext_.newPublicationHandler(
			[](const std::string &channel, std::int32_t streamId, std::int32_t sessionId, std::int64_t correlationId) {
				(void)channel; (void)streamId; (void)sessionId; (void)correlationId;
			}
		);
		aeronContext_.newSubscriptionHandler(
			[](const std::string &channel, std::int32_t streamId, std::int64_t correlationId) {
				(void)channel; (void)streamId; (void)correlationId;
			}
		);
		aeronContext_.availableImageHandler([](aeron::Image &image) { (void)image; });
		aeronContext_.unavailableImageHandler([](aeron::Image &image) {  (void)image; });
	
		aeronConnection_ = connection;
		aeronFragmentAssembler_ = std::make_shared<aeron::FragmentAssembler>(handleMessage());
		aeronHandler_ = std::make_shared<aeron::fragment_handler_t>(aeronFragmentAssembler_->handler());
		aeronIdleStrategy_ = std::make_shared<IdleStrategy>(idleStrategy);
	};

	~AeronClient() = default;


	/**
	 * @brief Connects to the Aeron media driver and sets up publications and subscriptions.
	 * 
	 * @param subscriptionIds A vector of subscription IDs to connect to.
	 * @param publicationIds A vector of publication IDs to connect to.
	 * @return Returns 0 on success, or a negative error code on failure.
	 */
	int connect(const std::vector<uint32_t>& subscriptionIds, const std::vector<uint32_t>& publicationIds) override {
		aeron_ = aeron::Aeron::connect(aeronContext_);
		int64_t id;
		
		try {
			for (const auto &pubId : publicationIds) {
				id = aeron_->addPublication(aeronConnection_, pubId);
				std::shared_ptr<aeron::Publication> publication = aeron_->findPublication(id);
				const auto deadline = std::chrono::steady_clock::now() + STREAM_CONNECTION_TIMEOUT;
				while (!publication) {
					if (std::chrono::steady_clock::now() > deadline) {
						std::cerr << "Aeron connection error: Timeout while waiting for publication" << std::endl;
						return -1; // Error: Timeout
					}
					std::this_thread::yield();
					publication = aeron_->findPublication(id);
				}
				aeronPublications_[pubId] = publication;
			}
			
			for (const auto &subId : subscriptionIds) {
				id = aeron_->addSubscription(aeronConnection_, subId);
				std::shared_ptr<aeron::Subscription> subscription = aeron_->findSubscription(id);
				const auto deadline = std::chrono::steady_clock::now() + STREAM_CONNECTION_TIMEOUT;
				while (!subscription) {
					if (std::chrono::steady_clock::now() > deadline) {
						std::cerr << "Aeron connection error: Timeout while waiting for subscription" << std::endl;
						return -1; // Error: Timeout
					}
					std::this_thread::yield();
					subscription = aeron_->findSubscription(id);
				}
				aeronSubscriptions_[subId] = subscription;
				aeronPolling_[subId] = false;
			}
		} catch (const std::exception &e) {
			std::cerr << "Aeron connection error: " << e.what() << std::endl;
			return -1; // Error: Aeron connection failed
		}

		if (aeronPublications_.empty() || aeronSubscriptions_.empty()) {
			std::cerr << "Aeron connection error: No publications or subscriptions available" << std::endl;
			return -1; // Error: No publications or subscriptions available
		}
		return 0;
	};


	/**
	 * @brief Sends a message over Aeron.
	 * 
	 * @param channelId The channel ID to send the message to.
	 * @param messageBytes The message bytes to send.
	 * @return Returns a positive number on success, or a negative error code on failure.
	 * (NOT_CONNECTED = -1, BACK_PRESSURED = -2, ADMIN_ACTION = -3, PUBLICATION_CLOSED = -4)
	 */
	int sendMessage(const uint32_t channelId, std::span<const uint8_t> messageBytes) override {
		const aeron::concurrent::AtomicBuffer srcBuffer(const_cast<uint8_t *>(messageBytes.data()), messageBytes.size());
		const auto it = aeronPublications_.find(channelId);
		if (it == aeronPublications_.end()) {
			std::cerr << "Aeron send error: Channel ID not found" << std::endl;
			return -1; // Error: Channel ID not found
		}
		return it->second->offer(srcBuffer, 0, messageBytes.size());
	};


	/**
	 * @brief Retrieves the last received message from Aeron.
	 * 
	 * @param channelId The channel ID to wait for a message from.
	 * @param timeout Maximum time to wait for a message before timing out.
	 * @return Bytes of the last message received. Returns an empty span on timeout or error.
	 */
	std::span<const uint8_t> waitForMessage(const uint32_t channelId, std::chrono::nanoseconds timeout = std::chrono::nanoseconds(0)) override {
		const auto it = aeronSubscriptions_.find(channelId);
		if (it == aeronSubscriptions_.end()) {
			std::cerr << "Aeron wait error: Channel ID not found" << std::endl;
			return {}; // Error: Channel ID not found
		}
		aeronPolling_[channelId] = true;
		while (aeronPolling_[channelId]) {
			const int fragmentsRead = it->second->poll(*aeronHandler_, 10);
			if(aeronIdleStrategy_->idle(fragmentsRead, timeout) != 0) {
				aeronIdleStrategy_->reset();
				return {}; // Error: Aeron message wait timed out
			}
		}
		aeronIdleStrategy_->reset();
		const auto& msg = aeronMessages_[channelId];
		return std::span<const uint8_t>(msg.data, msg.length);
	};


	/**
	 * @brief Waits for any Aeron message from all subscriptions.
	 * 
	 * @return Bytes of the last message received from any subscription. Returns an empty span on timeout or error.
	 */
	std::span<const uint8_t> waitForAnyMessage() override {
		for (const auto &channelId: aeronSubscriptions_ | std::views::keys) {
			aeronPolling_[channelId] = true;
		}
		while (true) {
			for (const auto &[channelId, subscription] : aeronSubscriptions_) {
				if (aeronPolling_[channelId]) {
					const int fragmentsRead = subscription->poll(*aeronHandler_, POLL_FRAGMENTS_LIMIT);
					if(aeronIdleStrategy_->idle(fragmentsRead) != 0) {
						aeronIdleStrategy_->reset();
						return {}; // Error: Aeron message wait timed out
					}
				} else {
					aeronIdleStrategy_->reset();
					const auto& msg = aeronMessages_[channelId];
					return std::span<const uint8_t>(msg.data, msg.length);
				}
			}
		}
	};

private:
	aeron::fragment_handler_t handleMessage() {
		return [&](const aeron::AtomicBuffer &buffer, aeron::util::index_t offset, aeron::util::index_t length, const aeron::Header &header) {
			aeronMessages_[header.streamId()] = {reinterpret_cast<const uint8_t *>(buffer.buffer() + offset), length};
			aeronPolling_[header.streamId()] = false;
		};
	};

	std::string aeronConnection_ {};
	aeron::Context aeronContext_ {};
	std::shared_ptr<aeron::Aeron> aeron_ {nullptr};
	std::unordered_map<uint32_t, std::shared_ptr<aeron::Publication>> aeronPublications_;
	std::unordered_map<uint32_t, std::shared_ptr<aeron::Subscription>> aeronSubscriptions_;
	std::shared_ptr<aeron::FragmentAssembler> aeronFragmentAssembler_ {nullptr};
	std::shared_ptr<aeron::fragment_handler_t> aeronHandler_ {nullptr};
	std::shared_ptr<IdleStrategy> aeronIdleStrategy_ {nullptr};
	/// Indicates if the Aeron client is currently polling for messages for the given channel ID
	std::unordered_map<uint32_t, std::atomic<bool>> aeronPolling_ {};
	/// Last message for each channel ID received from Aeron
	std::unordered_map<uint32_t, MessageView> aeronMessages_ {};
};

}
