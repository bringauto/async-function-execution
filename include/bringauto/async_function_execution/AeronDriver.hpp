#pragma once

#include <aeronmd/aeron_driver.h>



namespace bringauto::async_function_execution {

/**
 * This class provides access to a runnable Aeron media driver necessary for communication.
 */
class AeronDriver {

public:
	/**
	 * @brief Constructs Aeron Driver.
	 * Initializes the driver with default settings.
	 */
	AeronDriver();

	~AeronDriver() = default;

	/**
	 * @brief Starts the Aeron Driver.
	 * This method will block until the driver is stopped or an error occurs.
	 */
	void run();

	/**
	 * @brief Checks if the Aeron Driver is running.
	 * @return true if the driver is running, false otherwise.
	 */
	bool isRunning() const;

	/**
	 * @brief Stops the Aeron Driver.
	 * This method will stop the driver and release all resources.
	 */
	void stop();

private:
	/// Context for the Aeron driver. Driver settings can be adjusted in this struct.
	aeron_driver_context_t *driverContext_;
	/// Pointer to the Aeron driver instance.
	aeron_driver_t *driver_ {nullptr};
	/// Variable used in aeron macros that returns error codes.
	volatile int exitStatus_  {AERON_NULL_VALUE};

};

}