add_executable(idem_cli idem/main.cpp idem/options.cpp)
set_target_properties(idem_cli PROPERTIES OUTPUT_NAME idem)
target_link_libraries(idem_cli PRIVATE idem::idem)
idem_target_warnings(idem_cli)
