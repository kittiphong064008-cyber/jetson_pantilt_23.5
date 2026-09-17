cmake_minimum_required(VERSION 3.10)
project(jetson_pantilt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

find_path(CUDA_INCLUDE_DIR cuda_runtime_api.h
    PATHS /usr/local/cuda/include /usr/include
)
find_library(CUDA_LIBRARIES cudart
    PATHS /usr/local/cuda/lib64 /usr/lib/aarch64-linux-gnu /usr/lib/x86_64-linux-gnu
)
find_library(NVINFER_LIB nvinfer
    PATHS /usr/local/cuda/lib64 /usr/lib/aarch64-linux-gnu /usr/lib/x86_64-linux-gnu
)

find_package(X11 QUIET)
if(X11_FOUND)
    add_executable(jetson_pantilt
        src/main.cpp
        src/Keyboard.cpp
        src/Packet.cpp
        src/SerialLink.cpp
    )
    target_include_directories(jetson_pantilt PRIVATE include ${X11_INCLUDE_DIR})
    target_link_libraries(jetson_pantilt PRIVATE Threads::Threads ${X11_LIBRARIES})
    target_compile_options(jetson_pantilt PRIVATE -Wall -Wextra)
endif()

find_package(Qt6 COMPONENTS Widgets QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Widgets QUIET)
endif()

if(Qt6_FOUND OR Qt5_FOUND)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_INCLUDE_CURRENT_DIR ON)

    add_executable(jetson_pantilt_ui
        src/qt_main.cpp
        src/MainWindow.cpp
        src/SettingsDialog.cpp
        src/V4L2Camera.cpp
        src/JoystickWidget.cpp
        src/TrtYolo.cpp
        src/TrtClassifier.cpp
        src/Packet.cpp
        src/SerialLink.cpp
        src/TargetTracker.cpp
        src/HudRenderer.cpp
        src/TargetSelector.cpp
        src/GeoEstimator.cpp
        src/CommandArbitrator.cpp
        include/MainWindow.h
        include/SettingsDialog.h
        include/V4L2Camera.h
        include/JoystickWidget.h
        include/TrtYolo.h
        include/TrtClassifier.h
        include/PIDController.h
        include/TargetTracker.h
        include/HudRenderer.h
        include/TargetSelector.h
        include/GeoEstimator.h
        include/CommandArbitrator.h
    )

    target_include_directories(jetson_pantilt_ui PRIVATE
        include
        ${CUDA_INCLUDE_DIR}
        /usr/local/cuda/include
        /usr/include/aarch64-linux-gnu
    )
    target_compile_options(jetson_pantilt_ui PRIVATE -Wall -Wextra)

    set(EXTRA_LIBS Threads::Threads)
    if(CUDA_LIBRARIES)
        list(APPEND EXTRA_LIBS ${CUDA_LIBRARIES})
    endif()
    if(NVINFER_LIB)
        list(APPEND EXTRA_LIBS ${NVINFER_LIB})
    endif()

    if(Qt6_FOUND)
        target_link_libraries(jetson_pantilt_ui PRIVATE
            Qt6::Widgets ${EXTRA_LIBS})
    else()
        target_link_libraries(jetson_pantilt_ui PRIVATE
            Qt5::Widgets ${EXTRA_LIBS})
    endif()
else()
    message(STATUS "Qt not found — skipping jetson_pantilt_ui target.")
endif()
