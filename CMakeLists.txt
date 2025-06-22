cmake_minimum_required(VERSION 3.19)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)

# Include directories
include_directories( 
    "${MAX_SDK_INCLUDES}"
    "${MAX_SDK_MSP_INCLUDES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../tides_source"
)

# Source files (using our simplified Tides implementation)
file(GLOB PROJECT_SRC "*.h" "*.c" "*.cpp")

# Create the library
add_library(${PROJECT_NAME} MODULE ${PROJECT_SRC})

# Set C++ standard 
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)