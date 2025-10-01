#pragma once

#include <concurrent/Atomic64.h>
#include <util/BitUtil.h>

#include <thread>
#include <chrono>
#include <algorithm>



namespace bringauto::async_function_execution { 

class TimeoutIdleStrategy {
public:
	/**
	 * @brief Constructs a TimeoutIdleStrategy instance.
	 * 
	 * @param timeoutNs Optional default timeout duration for idling operations. If set to zero, no timeout is applied.
	 * @param maxSpinPeriodNs Maximum duration to spend in the SPINNING state. Default is 1 second.
	 * @param maxYieldPeriodNs Maximum duration to spend in the YIELDING state. Default is 2 seconds.
	 * @param minParkPeriodNs Minimum duration to sleep in the PARKING state. Default is 1 microsecond.
	 * @param maxParkPeriodNs Maximum duration to sleep in the PARKING state. Default is 1 millisecond.
	 */
	explicit TimeoutIdleStrategy(
		std::chrono::nanoseconds timeoutNs = std::chrono::nanoseconds(0),
		std::chrono::nanoseconds maxSpinPeriodNs = std::chrono::duration<long, std::milli>(1000),
		std::chrono::nanoseconds maxYieldPeriodNs = std::chrono::duration<long, std::milli>(2000),
		std::chrono::nanoseconds minParkPeriodNs = std::chrono::nanoseconds(1000),
		std::chrono::nanoseconds maxParkPeriodNs = std::chrono::duration<long, std::milli>(1)
	) : maxSpinPeriodNs_(maxSpinPeriodNs),
		maxYieldPeriodNs_(maxYieldPeriodNs),
		minParkPeriodNs_(minParkPeriodNs),
		maxParkPeriodNs_(maxParkPeriodNs),
		parkPeriodNs_(minParkPeriodNs),
		state_(IdleState::NOT_IDLE),
		timeoutNs_(timeoutNs) {}

	/**
	 * @brief Idles based on the current state and the provided timeout.
	 * 
	 * @param workCount Number of work items processed since the last call.
	 * @param timeout Optional timeout duration. If not provided, the default timeout set in the
	 * configuration will be used.
	 * @return 0 if idling continues, -1 if the timeout has been reached.
	 */
	int idle(const int workCount, std::chrono::nanoseconds timeout = std::chrono::nanoseconds(0));

	/**
	 * @brief Resets the idle strategy to its initial state.
	 */
	void reset();

private:
	/**
	 * @brief Enumeration of idle states.
	 */
	enum class IdleState {
		/// Default state, no idling.
		NOT_IDLE = 0,
		/// Spinning state, actively checking for work.
		SPINNING = 1,
		/// Yielding state, giving up the CPU for a short duration.
		YIELDING = 2,
		/// Parking state, sleeping for a longer duration.
		PARKING = 3
	};

	int idle(std::chrono::nanoseconds timeout = std::chrono::nanoseconds(0));

	std::chrono::nanoseconds maxSpinPeriodNs_;
	std::chrono::nanoseconds maxYieldPeriodNs_;
	std::chrono::nanoseconds minParkPeriodNs_;
	std::chrono::nanoseconds maxParkPeriodNs_;
	std::chrono::nanoseconds parkPeriodNs_;
	IdleState state_;
	std::chrono::nanoseconds timeoutNs_;
	std::chrono::steady_clock::time_point startTime_;
	std::chrono::steady_clock::time_point lastStateSwitchTime_;
};

}
