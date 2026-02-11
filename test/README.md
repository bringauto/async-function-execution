# Async Function Execution - TESTS

## Requirements

[gtest](https://github.com/google/googletest)

## Build

Tests are built as part of the main project. Don't use the CMakeLists file in the test folder.

```bash
mkdir -p _build_tests && cd _build_tests
cmake ../ -DBRINGAUTO_TESTS=ON
make
```

## Run unit tests

```bash
./async_function_execution_tests
```

## Run integration tests

### Simple function test

A test that checks if the basic functionality works over aeron communication.

```bash
./async_function_execution_simple_function_test
```

### Segfault test

To test if a segfault in the consumer affects the producer first run:

```bash
./async_function_execution_segfault_producer_test
```

then, when prompted, run:

```bash
./async_function_execution_segfault_consumer_test
```
