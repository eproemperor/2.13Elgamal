cmake_minimum_required(VERSION 3.8)
project(elgamal_client)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rm_server REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(elgamal_client src/elgamal_client.cpp)
ament_target_dependencies(elgamal_client rclcpp rm_server std_msgs)

install(TARGETS elgamal_client
  DESTINATION lib/${PROJECT_NAME})

ament_package()
