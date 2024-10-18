#####################
# rpi_security_system
#####################

RPI_SECURITY_SYSTEM_VERSION = 1.0
RPI_SECURITY_SYSTEM_SITE = $(TOPDIR)/../rpi_security_system_src
RPI_SECURITY_SYSTEM_SITE_METHOD = local
RPI_SECURITY_SYSTEM_LICENSE = MIT
RPI_SECURITY_SYSTEM_DEPENDENCIES = c-periphery

$(eval $(cmake-package))
