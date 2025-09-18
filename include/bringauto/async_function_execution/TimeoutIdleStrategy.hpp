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
		std::chrono::nanoseconds maxSpinPeriodNs = std::chrono::duration<long, std::milli>(1000),
		std::chrono::nanoseconds maxYieldPeriodNs = std::chrono::duration<long, std::milli>(2000),
		std::chrono::nanoseconds minParkPeriodNs = std::chrono::nanoseconds(1000),
		std::chrono::nanoseconds maxParkPeriodNs = std::chrono::duration<long, std::milli>(1)
	) : prePad_(),
		maxSpinPeriodNs_(maxSpinPeriodNs),
		maxYieldPeriodNs_(maxYieldPeriodNs),
		minParkPeriodNs_(minParkPeriodNs),
		maxParkPeriodNs_(maxParkPeriodNs),
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
		parkPeriodNs_ = minParkPeriodNs_;
		state_ = IdleState::NOT_IDLE;
		startTime_ = std::chrono::steady_clock::time_point();
	}

	int idle() {
		auto now = std::chrono::steady_clock::now();
		if (timeoutNs_ != std::chrono::nanoseconds(0)) {
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
				lastStateSwitchTime_ = now;
				break;

			case IdleState::SPINNING:
				aeron::concurrent::atomic::cpu_pause();
				if (std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastStateSwitchTime_) >= maxSpinPeriodNs_) {
					state_ = IdleState::YIELDING;
					lastStateSwitchTime_ = now;
				}
				break;

			case IdleState::YIELDING:
				std::this_thread::yield();
				if (std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastStateSwitchTime_) >= maxYieldPeriodNs_) {
					state_ = IdleState::PARKING;
					parkPeriodNs_ = minParkPeriodNs_;
					lastStateSwitchTime_ = now;
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
	std::chrono::nanoseconds maxSpinPeriodNs_;
	std::chrono::nanoseconds maxYieldPeriodNs_;
	std::chrono::nanoseconds minParkPeriodNs_;
	std::chrono::nanoseconds maxParkPeriodNs_;
	std::chrono::nanoseconds parkPeriodNs_;
	IdleState state_;
	std::chrono::nanoseconds timeoutNs_;
	std::chrono::steady_clock::time_point startTime_;
	std::chrono::steady_clock::time_point lastStateSwitchTime_;
	std::uint8_t postPad_[aeron::util::BitUtil::CACHE_LINE_LENGTH];
};

}
