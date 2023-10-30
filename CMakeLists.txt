# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# Define the project and set it to use C as the language
project(TestChatProject C)

# Add include directories
include_directories(chat)

# Create an executable
add_executable(server chat/test_chat_server.c chat/chat.c)

# Link libraries
target_link_libraries(server pthread)
