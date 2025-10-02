#include <bringauto/async_function_execution/TimeoutIdleStrategy.hpp>

#include <concurrent/Atomic64.h>

#include <thread>



namespace bringauto::async_function_execution {

int TimeoutIdleStrategy::idle(const int workCount, std::chrono::nanoseconds timeout) {
	if (workCount > 0) {
		reset();
		return 0;
	}
	return idle(timeout);
}

void TimeoutIdleStrategy::reset() {
	parkPeriodNs_ = minParkPeriodNs_;
	state_ = IdleState::NOT_IDLE;
	startTime_ = std::chrono::steady_clock::time_point();
}

int TimeoutIdleStrategy::idle(std::chrono::nanoseconds timeout) {
	auto timeoutNs = timeout != std::chrono::nanoseconds(0) ? timeout : timeoutNs_;
	auto now = std::chrono::steady_clock::now();
	if (timeoutNs != std::chrono::nanoseconds(0)) {
		if (startTime_ == std::chrono::steady_clock::time_point()) {
			startTime_ = now;
		}
		if (std::chrono::duration_cast<std::chrono::nanoseconds>(now - startTime_) >= timeoutNs) {
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

}
