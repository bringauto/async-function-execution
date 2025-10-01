# Async function execution example

This folder contains a simple example of the usage of this project. 3 executables will be built: the aeron driver, the producer and the consumer.

## Build

Examples are built as part of the main project. Don't use the CMakeList in the test folder.

```bash
mkdir -p _build_example && cd _build_example
cmake ../ -DBRINGAUTO_SAMPLES=ON
make
```

## Run

```bash
# Run the executables in this order
./example_driver
./example_consumer
./example_producer
```