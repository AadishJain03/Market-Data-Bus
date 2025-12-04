cmake_minimum_required(VERSION 3.16)

add_library(md-bus-engine STATIC
  bus/bus.cpp
  record/recorder.cpp
  replay/replay.cpp
)

target_include_directories(md-bus-engine
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(md-bus-engine PUBLIC cxx_std_17)

# Link fmt library to md-bus-engine
target_link_libraries(md-bus-engine PUBLIC fmt::fmt)

add_executable(hello_bus
  examples/hello_bus.cpp
)

target_link_libraries(hello_bus PRIVATE md-bus-engine)

add_executable(example_replay
    examples/replay_hello.cpp
)

target_link_libraries(example_replay PRIVATE md-bus-engine)
# NOTE: previously linked to md_engine (typo). Link against md-bus-engine instead.

add_executable(example_check_log
    examples/check_log.cpp
)

target_link_libraries(example_check_log
    PRIVATE md-bus-engine   # or whatever your main engine lib target is called
)

add_executable(example_replay_filters
    examples/replay_filters.cpp
)

target_link_libraries(example_replay_filters
    PRIVATE md-bus-engine  # adjust to your main library target name
)

add_executable(example_strategy_hello
    examples/strategy_hello.cpp
)

target_link_libraries(example_strategy_hello
    PRIVATE md-bus-engine
)

add_executable(example_stateful_strategy
    examples/stateful_strategy.cpp
)

target_link_libraries(example_stateful_strategy
    PRIVATE md-bus-engine
)

add_compile_definitions(BUS_DEBUG)