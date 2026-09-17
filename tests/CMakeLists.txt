find_package(Qt6 REQUIRED COMPONENTS Test)

# Sources with Q_OBJECT (fakes.h) must be listed so automoc sees them.
function(t2t_add_test name lib)
  qt_add_executable(${name} ${name}.cpp ${ARGN})
  target_link_libraries(${name} PRIVATE ${lib} Qt6::Test)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

t2t_add_test(tst_settings t2t_core)
t2t_add_test(tst_mapper t2t_core)
t2t_add_test(tst_gesture t2t_core)
t2t_add_test(tst_daemon t2t_daemonlib fakes.h)
t2t_add_test(tst_control t2t_daemonlib fakes.h)
t2t_add_test(tst_panels t2t_daemonlib)
target_compile_definitions(tst_panels PRIVATE T2T_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
