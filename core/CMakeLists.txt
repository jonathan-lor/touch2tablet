# Platform-neutral core: settings, mapping and the gesture machine.  QtCore only.
add_library(t2t_core STATIC
  src/Settings.cpp
  src/Mapper.cpp
  src/GestureMachine.cpp
  src/SettingsStore.cpp
)
target_include_directories(t2t_core PUBLIC include)
target_link_libraries(t2t_core PUBLIC Qt6::Core)
