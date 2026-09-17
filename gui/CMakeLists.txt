# Qt Widgets front end.
add_library(t2t_guilib STATIC
  src/ControlClient.cpp
  src/ControlClient.h
  src/AreaCanvas.cpp
  src/AreaCanvas.h
  src/MainWindow.cpp
  src/MainWindow.h
)
target_include_directories(t2t_guilib PUBLIC src)
target_link_libraries(t2t_guilib PUBLIC t2t_core Qt6::Widgets Qt6::Network)

qt_add_executable(touch2tablet src/main.cpp)
target_link_libraries(touch2tablet PRIVATE t2t_guilib)
