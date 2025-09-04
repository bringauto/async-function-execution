#include <bringauto/async_function_execution/AeronDriver.hpp>

#include <signal.h>



static bringauto::async_function_execution::AeronDriver* globalDriverInstance = nullptr;

void terminationHook(void *state) {
	if (globalDriverInstance != nullptr) {
		globalDriverInstance->stop();
	}
}

void signalHandler(int signal) {
	if (globalDriverInstance != nullptr) {
		globalDriverInstance->stop();	
	}
}


namespace bringauto::async_function_execution {

AeronDriver::AeronDriver() {
	globalDriverInstance = this;
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	aeron_driver_context_init(&driverContext_);
	aeron_driver_context_set_driver_termination_hook(driverContext_, terminationHook, NULL);
	driverContext_->agent_on_start_func_delegate = driverContext_->agent_on_start_func;
	driverContext_->agent_on_start_state_delegate = driverContext_->agent_on_start_state;
	aeron_driver_context_set_agent_on_start_function(driverContext_, aeron_set_thread_affinity_on_start, driverContext_);
	aeron_driver_init(&driver_, driverContext_);
}


void AeronDriver::run() {
	aeron_driver_start(driver_, true);
	while (isRunning()) {
		aeron_driver_main_idle_strategy(driver_, aeron_driver_main_do_work(driver_));
	}
}


bool AeronDriver::isRunning() const {
	int result;
	AERON_GET_ACQUIRE(result, exitStatus_);
	return result == AERON_NULL_VALUE;
}


void AeronDriver::stop() {
	AERON_SET_RELEASE(exitStatus_, EXIT_SUCCESS);
}

}
