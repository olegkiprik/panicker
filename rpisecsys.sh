#!/bin/bash
export RPI_SEC_SYS_PROJ_MAIN_DIR=`pwd`
mkdir ccache_br
git clone --single-branch --branch 2024.02.x --depth 1 git://git.buildroot.net/buildroot
cd buildroot
make raspberrypi4_64_defconfig
cd ..
cp BR_config buildroot/.config
cp BR_genimage buildroot/board/raspberrypi4-64/genimage.cfg.in
cp -r rpi_security_system buildroot/package/
cp BR_pkg_config buildroot/package/Config.in
gcc server.c -o server
cd buildroot
time make -j$(($(nproc) + 1))
cd ..
./server &
echo "./server has been started in background"
