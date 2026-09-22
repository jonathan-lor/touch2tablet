# Daemon: portable orchestration and control server, with optional Linux hardware support.
set(T2T_LINUX_BACKEND_DEFAULT OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(T2T_LINUX_BACKEND_DEFAULT ON)
endif()
option(T2T_ENABLE_LINUX_BACKEND "Build evdev/uinput hardware support" ${T2T_LINUX_BACKEND_DEFAULT})
if(T2T_ENABLE_LINUX_BACKEND AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "T2T_ENABLE_LINUX_BACKEND requires Linux")
endif()

add_library(t2t_daemonlib STATIC
  src/Daemon.cpp
  src/Daemon.h
  src/ControlServer.cpp
  src/ControlServer.h
  src/ITouchSource.h
  src/IPenSink.h
  src/Panels.cpp
  src/Panels.h
  src/TouchTypes.h
)
target_include_directories(t2t_daemonlib PUBLIC src)
target_link_libraries(t2t_daemonlib PUBLIC t2t_core Qt6::Core Qt6::Network)
# The supported panels, read by Panels.cpp as :/panels.json.
qt_add_resources(t2t_daemonlib panels PREFIX / BASE ${PROJECT_SOURCE_DIR} FILES ${PROJECT_SOURCE_DIR}/panels.json)

qt_add_executable(touch2tabletd src/main.cpp)
target_link_libraries(touch2tabletd PRIVATE t2t_daemonlib)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources(touch2tabletd PRIVATE src/UnixSignals.cpp src/UnixSignals.h)
endif()

if(T2T_ENABLE_LINUX_BACKEND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBEVDEV REQUIRED IMPORTED_TARGET libevdev)
  target_sources(touch2tabletd PRIVATE
    src/backend/linux/EvdevTouchSource.cpp
    src/backend/linux/EvdevTouchSource.h
    src/backend/linux/UinputPenSink.cpp
    src/backend/linux/UinputPenSink.h
  )
  target_link_libraries(touch2tabletd PRIVATE PkgConfig::LIBEVDEV)
  target_compile_definitions(touch2tabletd PRIVATE T2T_ENABLE_LINUX_BACKEND)
else()
  message(STATUS "No hardware backend: touch2tabletd requires --no-device")
endif()
