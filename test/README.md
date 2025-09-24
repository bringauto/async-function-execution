# Async Function Execution - TESTS

## Requirements

[gtest](https://github.com/google/googletest)

## Build

```bash
mkdir -p _build_tests && cd _build_tests
cmake ../ -DCMLIB_DIR=<absolute path cmakelib> -DBRINGAUTO_TESTS=ON -DCMAKE_PREFIX_PATH=<path-to-aeron-install>
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
