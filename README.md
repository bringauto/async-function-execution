# Async Function Execution

A library providing the ability to call functions on a different binary over shared memory. The communication is divided to a producer and consumer.

### Implementation

Both sides need to have all functions defined the same way. Example implementation:

```cpp
#include <bringauto/async_function_execution/AsyncFunctionExecutor.hpp>

using namespace bringauto::async_function_execution;

FunctionDefinition FunctionAdd {
	FunctionId { 1 }, // id can be 0-999 and has to be unique
	Return { int {} }, // return type of the function
	Arguments { int {}, int {}, int {} } // individual function argument types
};

AsyncFunctionExecutor executorProducer {
	Config {
		.isProducer = true, // decides the mode of the executor
		.defaultTimeout = std::chrono::seconds(1) // polling timeout (should only be used when producer)
	},
	FunctionList { std::tuple{ // list of all functions
		FunctionAdd
	} }
};
```

### Producer

Producer is the side calling functions and waiting for a response from the consumer. If timeout is provided in config, the function will throw if it doesn't execute in time. Example of function calling:

```cpp
// the function definition and same number of expected arguments need to be provided
int ret = executorProducer.callFunc(FunctionAdd, 1, 2, 3);
```

### Consumer

Consumer is consistently polling function requests, executing the requested functions and sending the required return value back to the producer. Example function calling:

```cpp
while (true) {
	// poll for function requests and receive a function id and bytes holding the argument data
	auto [funcId, argBytes] = executorConsumer.pollFunction();
	// argument data then needs to be deserialized by providing the corresponding function definition
	auto [arg1, arg2, arg3] = executorConsumer.getFunctionArgs(FunctionAdd, argBytes);

	// do some work with the arguments and send a return value back to the producer
	int ret = arg1 + arg2 + arg3;
	executorConsumer.sendReturnMessage(FunctionAdd.id, ret);
}
```

### Non trivially copiable data types

If an argument or return type is not trivially copiable, a way of data serialization needs to be provided. This can be done simply by wraping the data type in a struct and implementing both serialize() and deserialize() functions. Example for std::string:

```cpp
struct SerializableString final {
	std::string value {};
	SerializableString() = default;
	SerializableString(std::string str) : value(std::move(str)) {}

	std::span<const uint8_t> serialize() const {
		return std::span {reinterpret_cast<const uint8_t *>(value.data()), value.size()};
	}
	void deserialize(std::span<const uint8_t> bytes) {
		value = std::string {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
	}
};
```

### Data lifetime

If a producer expects a return value where returned bytes are used directly, these bytes are valid untill the next call of the same function. If a longer lifespan is required, these bytes need to be copied, otherwise double free() errors might occur during runtime.

## Requirements

- [cmlib](https://github.com/cmakelib/cmakelib)
- [aeron](https://github.com/aeron-io/aeron)

### Aeron setup

Build and install aeron to any folder.

```bash
git clone https://github.com/aeron-io/aeron.git
cd aeron
git checkout 1.48.5
./cppbuild/cppbuild
cd cppbuild/Release
cmake --install . --prefix <absolute-path-to-install-folder>
```

## Build

```bash
mkdir -p _build && cd _build
cmake ../ -DCMLIB_DIR=<absolute-path-cmakelib> -DCMAKE_PREFIX_PATH=<path-to-aeron-install>
make
```

## Tests

[Tests Readme](./test/README.md)
