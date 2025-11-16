# Dùng include/ cho tất cả target
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    /usr/include/opencv4
)

# List OpenCV libs manually (dùng bản đã cài qua apt)
set(OPENCV_LIBS
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    opencv_highgui
    opencv_videoio
)

# Gom source cho server & client
file(GLOB_RECURSE SERVER_SRC
    ${PROJECT_SOURCE_DIR}/src/server/*.cpp
    ${PROJECT_SOURCE_DIR}/src/common/*.cpp
    ${PROJECT_SOURCE_DIR}/src/modules/*/*.cpp
    ${PROJECT_SOURCE_DIR}/src/main_server.cpp
)

file(GLOB_RECURSE CLIENT_SRC
    ${PROJECT_SOURCE_DIR}/src/client/*.cpp
    ${PROJECT_SOURCE_DIR}/src/common/*.cpp
    ${PROJECT_SOURCE_DIR}/src/modules/*/*.cpp
    ${PROJECT_SOURCE_DIR}/src/main_client.cpp
)

# Nếu Windows: thêm file ScreenCapturer_Windows.cpp riêng (nếu chưa nằm trong GLOB)
if (WIN32)
    list(APPEND SERVER_SRC
        ${PROJECT_SOURCE_DIR}/src/modules/screen/ScreenCapturer_Windows.cpp
    )
    list(APPEND CLIENT_SRC
        ${PROJECT_SOURCE_DIR}/src/modules/screen/ScreenCapturer_Windows.cpp
    )
endif()

# Tạo executable
add_executable(server ${SERVER_SRC})
add_executable(client ${CLIENT_SRC})

# Link thư viện chung
target_link_libraries(server
    PRIVATE
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        ${OPENCV_LIBS}
)

target_link_libraries(client
    PRIVATE
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        ${OPENCV_LIBS}
)

# Linux: link thêm X11 + Xext cho XShm
if (UNIX AND NOT APPLE)
    target_link_libraries(server PRIVATE X11 Xext)
    target_link_libraries(client PRIVATE X11 Xext)
endif()
