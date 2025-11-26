cmake_minimum_required(VERSION 3.16)

add_library(md-bus-engine STATIC
  bus/bus.cpp
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

add_compile_definitions(BUS_DEBUG)