# Platform-neutral core: settings model, mapper, gesture machine, protocol types.
# Depends on QtCore only.  No evdev, uinput or QtGui here.
add_library(t2t_core STATIC
  src/Settings.cpp
  src/Mapper.cpp
  src/GestureMachine.cpp
  src/SettingsStore.cpp
)
target_include_directories(t2t_core PUBLIC include)
target_link_libraries(t2t_core PUBLIC Qt6::Core)
