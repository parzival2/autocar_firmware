
#
# Components, types and plugins.
#
# The CMake 'target' names are identical to the first argument of the
# macros below, except for orocos_typegen_headers, where the target is fully
# controlled by generated code of 'typegen'.
#

# Creates a component library libicm20948_rtt_pigpio-<target>.so
# and installs in the directory lib/orocos/icm20948_rtt_pigpio/
#
orocos_component(icm20948_rtt_pigpio icm20948_rtt_pigpio-component.hpp icm20948_rtt_pigpio-component.cpp) # ...you may add multiple source files
#
# You may add multiple orocos_component statements.

#
# Additional headers:
#
# Installs in the include/orocos/icm20948_rtt_pigpio/ directory
#
orocos_install_headers(icm20948_rtt_pigpio-component.hpp) # ...you may add multiple header files
#
# You may add multiple orocos_install_headers statements.
target_link_libraries(icm20948_rtt_pigpio -lpigpiod_if2 -lrt)

