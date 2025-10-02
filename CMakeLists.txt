cmake_minimum_required(VERSION 3.16)
project(SimpleChat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 path
set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt")

# Enable automatic moc, uic, and rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6 modules
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# Add executable
add_executable(SimpleChat
    main.cpp
    ChatWindow.cpp
    ChatWindow.h
)

# Link Qt6 libraries
target_link_libraries(SimpleChat
    PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
)
