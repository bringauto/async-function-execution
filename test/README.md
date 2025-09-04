# Async Function Execution - TESTS

## Requirements

[gtest](https://github.com/google/googletest)

## Build
```
mkdir -p _build_tests && cd _build_tests
cmake ../ -DCMLIB_DIR=<absolute path cmakelib> -DBRINGAUTO_TESTS=ON -DCMAKE_PREFIX_PATH=<path-to-aeron-install>
make
```

## Run
```
./async_function_execution_tests
```
