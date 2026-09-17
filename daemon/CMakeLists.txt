# Daemon: orchestration library (testable with fake backends) + Linux executable.
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

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED IMPORTED_TARGET libevdev)

qt_add_executable(touch2tabletd
  src/main.cpp
  src/UnixSignals.cpp
  src/UnixSignals.h
  src/backend/linux/EvdevTouchSource.cpp
  src/backend/linux/EvdevTouchSource.h
  src/backend/linux/UinputPenSink.cpp
  src/backend/linux/UinputPenSink.h
)
target_link_libraries(touch2tabletd PRIVATE t2t_daemonlib PkgConfig::LIBEVDEV)
