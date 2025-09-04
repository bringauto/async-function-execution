#pragma once

#include <concurrent/Atomic64.h>
#include <util/BitUtil.h>

#include <thread>
#include <chrono>
#include <algorithm>



namespace bringauto::async_function_execution { 

enum class IdleState {
	NOT_IDLE = 0,
	SPINNING = 1,
	YIELDING = 2,
	PARKING = 3
};

class TimeoutIdleStrategy {
public:
	explicit TimeoutIdleStrategy(
		std::chrono::nanoseconds timeoutNs = std::chrono::nanoseconds(0),
		std::int64_t maxSpins = 10,
		std::int64_t maxYields = 20,
		std::chrono::duration<long, std::nano> minParkPeriodNs = std::chrono::duration<long, std::nano>(1000),
		std::chrono::duration<long, std::nano> maxParkPeriodNs = std::chrono::duration<long, std::milli>(1)
	) : prePad_(),
		maxSpins_(maxSpins),
		maxYields_(maxYields),
		minParkPeriodNs_(minParkPeriodNs),
		maxParkPeriodNs_(maxParkPeriodNs),
		spins_(0),
		yields_(0),
		parkPeriodNs_(minParkPeriodNs),
		state_(IdleState::NOT_IDLE),
		timeoutNs_(timeoutNs),
		postPad_() {}

	int idle(const int workCount) {
		if (workCount > 0) {
			reset();
			return 0;
		}
		return idle();
	}

	void reset() {
		spins_ = 0;
		yields_ = 0;
		parkPeriodNs_ = minParkPeriodNs_;
		state_ = IdleState::NOT_IDLE;
		startTime_ = std::chrono::steady_clock::time_point();
	}

	int idle() {
		if (timeoutNs_ != std::chrono::nanoseconds(0)) {
			auto now = std::chrono::steady_clock::now();
			if (startTime_ == std::chrono::steady_clock::time_point()) {
				startTime_ = now;
			}
			if (std::chrono::duration_cast<std::chrono::nanoseconds>(now - startTime_) >= timeoutNs_) {
				return -1;
			}
		}

		switch(state_) {
			case IdleState::NOT_IDLE:
				state_ = IdleState::SPINNING;
				spins_++;
				break;

			case IdleState::SPINNING:
				aeron::concurrent::atomic::cpu_pause();
				if (++spins_ > maxSpins_) {
					state_ = IdleState::YIELDING;
					yields_ = 0;
				}
				break;

			case IdleState::YIELDING:
				if (++yields_ > maxYields_) {
					state_ = IdleState::PARKING;
					parkPeriodNs_ = minParkPeriodNs_;
				} else {
					std::this_thread::yield();
				}
				break;

			case IdleState::PARKING:
			default:
				std::this_thread::sleep_for(parkPeriodNs_);
				parkPeriodNs_ = std::min(parkPeriodNs_ * 2, maxParkPeriodNs_);
				break;
		}
		return 0;
	}

	void setTimeout(std::chrono::nanoseconds timeoutNs) {
		timeoutNs_ = timeoutNs;
	}

protected:
	std::uint8_t prePad_[aeron::util::BitUtil::CACHE_LINE_LENGTH];
	std::int64_t maxSpins_;
	std::int64_t maxYields_;
	std::chrono::duration<long, std::nano> minParkPeriodNs_;
	std::chrono::duration<long, std::nano> maxParkPeriodNs_;
	std::int64_t spins_;
	std::int64_t yields_;
	std::chrono::duration<long, std::nano> parkPeriodNs_;
	IdleState state_;
	std::chrono::nanoseconds timeoutNs_;
	std::chrono::steady_clock::time_point startTime_;
	std::uint8_t postPad_[aeron::util::BitUtil::CACHE_LINE_LENGTH];
};

}
