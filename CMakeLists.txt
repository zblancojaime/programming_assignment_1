cmake_minimum_required(VERSION 3.16)
project(SimpleChat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 installation path
set(Qt6_DIR "/opt/homebrew/opt/qt/lib/cmake/Qt6")

# Find Qt6 modules
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Source files
set(SOURCES
    main.cpp
    ChatWindow.cpp
)

# Header files that require moc (Q_OBJECT)
set(HEADERS
    ChatWindow.h
)

# Generate moc files from headers
qt6_wrap_cpp(MOC_FILES ${HEADERS})

# Create the executable including moc files
add_executable(SimpleChat
    ${SOURCES}
    ${MOC_FILES}   # Include generated moc files
)

# Link Qt6 libraries
target_link_libraries(SimpleChat
    PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets
)
