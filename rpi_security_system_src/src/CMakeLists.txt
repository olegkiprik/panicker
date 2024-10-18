add_executable (rpi_security_system main.c)
install (TARGETS rpi_security_system DESTINATION bin)
target_link_libraries (rpi_security_system periphery m)
