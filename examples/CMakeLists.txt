CMAKE_MINIMUM_REQUIRED(VERSION 3.25 FATAL_ERROR)

IF(NOT TARGET async-function-execution-shared)
    MESSAGE(FATAL_ERROR "The async-function-execution-shared target was not found. Please build the example as part of the async-function-execution-shared project.")
ENDIF()

ADD_EXECUTABLE(example_producer main_producer.cpp)
ADD_EXECUTABLE(example_consumer main_consumer.cpp)
ADD_EXECUTABLE(example_driver main_driver.cpp)

TARGET_LINK_LIBRARIES(example_producer PUBLIC async-function-execution-shared)
TARGET_LINK_LIBRARIES(example_consumer PUBLIC async-function-execution-shared)
TARGET_LINK_LIBRARIES(example_driver PUBLIC async-function-execution-shared)
