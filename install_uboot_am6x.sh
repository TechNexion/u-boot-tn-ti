#!/bin/sh
#################################################################################
# Copyright 2024 TechNexion Ltd.
#
# Author: Ray Chang <ray.chang@technexion.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#################################################################################

DRIVE=/dev/sdX
FOR_R5=0

K3IG_SRC="https://git.ti.com/git/k3-image-gen/k3-image-gen.git"
K3IG_BRANCH="master"
K3IG_DIR="k3-image-gen"
ATF_SRC="https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git/"
ATF_SRC_GIT_ID="2fcd408bb3a6756767a43c073c597cef06e7f2d5"	# meta-ti/recipes-bsp/trusted-firmware-a/trusted-firmware-a_%.bbappend
ATF_DIR="trusted-firmware-a"
OPTEE_SRC="https://github.com/OP-TEE/optee_os.git/"
OPTEE_SRC_GIT_ID="8e74d47616a20eaa23ca692f4bbbf917a236ed94"	# meta-ti/recipes-security/optee/optee-os_%.bbappend
OPTEE_DIR="optee_os"
FIRMWARE_SRC="https://git.ti.com/git/processor-firmware/ti-linux-firmware.git/"
FIRMWARE_BRANCH="ti-linux-firmware"
FIRMWARE_GIT_ID="2944354aca1f95525c30d625cb17672930e72572"	# meta-ti/recipes-bsp/ti-linux-fw/ti-linux-fw.inc
FIRMWARE_DIR="ti-linux-firmware"
SYSFW_DIR="ti-linux-firmware/ti-sysfw"
DMFW_DIR="ti-linux-firmware/ti-dm"

ATF_ORI="build/k3/lite/release/bl31.bin"
TEE_ORI="out/arm-plat-k3/core/tee-pager_v2.bin"
DM_ORI="ipc_echo_testb_mcu1_0_release_strip.xer5f"
MOUNT_BOOT_DIR="/media/$USER/boot"
TIBOOT3="tiboot3.bin"
TIBOOT3_SPL_ORI="spl/u-boot-spl.bin"
SPL_ORI="tispl.bin"
UBOOT_ORI="u-boot.img"
TWD=`pwd`

setup_platform()
{
	if [ ${SOC} = "am62xx" ] ; then
		PLATFORM="am62xx"
		SOC_TARGET=am62x
		SOC_TYPE=gp
	elif [ ${SOC} = "am62ax" ] ; then
		PLATFORM="am62axx"
		SOC_TARGET=am62ax
		SOC_TYPE=gp
	else
		printf "Targest SOC isn't supported by this script\n"
		exit 1
	fi
}

install_firmware()
{
	cd ${TWD}
	#Collect required firmware files to generate bootable binary
	if [ ! -d ${K3IG_DIR} ] ; then
		git clone ${K3IG_SRC} -b ${K3IG_BRANCH} || printf "Fails to fetch %s source code \n" ${K3IG_DIR}
	fi

	if [ ! -d ${ATF_DIR} ] ; then
		git clone ${ATF_SRC} || printf "Fails to fetch %s source code \n" ${ATF_DIR}
		cd ${ATF_DIR}
		git checkout ${ATF_SRC_GIT_ID}
		cd -
	fi

	if [ ! -d ${OPTEE_DIR} ] ; then
		git clone ${OPTEE_SRC} || printf "Fails to fetch %s source code \n" ${OPTEE_DIR}
		cd ${OPTEE_DIR}
		git checkout ${OPTEE_SRC_GIT_ID}
		cd -
	fi

	if [ ! -d ${FIRMWARE_DIR} ] ; then
		git clone ${FIRMWARE_SRC} -b ${FIRMWARE_BRANCH} || printf "Fails to fetch %s source code \n" ${FIRMWARE_DIR}
		cd ${FIRMWARE_DIR}
		git checkout ${FIRMWARE_GIT_ID}
		cd -
	fi

	if [ $FOR_R5 -eq 0 ]; then
		#Building ATF on GP
		if [ ! -f ${ATF_DIR}/${ATF_ORI} ] ; then
			cd ${ATF_DIR}
			rm -rf build
			make PLAT=k3 TARGET_BOARD=lite SPD=opteed || printf "Fails to build ATF firmware \n"
			cd -
		fi

		#Building OP-TEE on GP
		if [ ! -f ${OPTEE_DIR}/${TEE_ORI} ] ; then
			cd ${OPTEE_DIR}
			rm -rf out
			make CROSS_COMPILE64=aarch64-none-linux-gnu- PLATFORM=k3-am62x CFG_ARM64_core=y ta-targets=ta_arm64 || \
				printf "Fails to build OP-TEE firmware \n"
			cd -
		fi
	fi
}

generate_uboot_binary()
{
	if [ $FOR_R5 -eq 1 ]; then
		#To build tiboot3-am62ax-gp-evm.bin. Saved in $K3IG_DIR. Requires u-boot-spl.bin and ti-fs-firmware-am62ax-gp.bin.
		cd ${K3IG_DIR}
		[ -f $TIBOOT3 ] && make SOC=${SOC_TARGET} clean
		make SOC=${SOC_TARGET} SOC_TYPE="${SOC_TYPE}" SBL=${TWD}/${TIBOOT3_SPL_ORI} SYSFW_DIR=${TWD}/${SYSFW_DIR} && \
			printf "Make target: to build tiboot3.bin ... \n" || printf "Fails to build tiboot3.bin ... \n"
	else
		#Generate bootable binary (This binary contains tispl.bin and u-boot.img) for copying
		cd ${TWD}
		make ATF=${ATF_DIR}/${ATF_ORI} TEE=${OPTEE_DIR}/${TEE_ORI} DM=${DMFW_DIR}/${PLATFORM}/${DM_ORI} && \
			printf "Make target: generate bootable binary... \n" || printf "Fails to generate bootable binary... \n"
	fi
}

copy_uboot_binary()
{
	cd ${TWD}
	if [ ! -b $DRIVE ]; then
		echo "$DRIVE doesn't exist !!!"
		exit
	fi
	if [ ! -d $MOUNT_BOOT_DIR ]; then
		sudo mkdir -p ${MOUNT_BOOT_DIR}
		sudo chown ${USER}:${USER} ${MOUNT_BOOT_DIR}
		sudo mount -t vfat ${DRIVE}1 ${MOUNT_BOOT_DIR} -o uid=1000 -o gid=1000
	fi
	sleep 0.1
	if [ $FOR_R5 -eq 1 ]; then
		cp ${K3IG_DIR}/${TIBOOT3} ${MOUNT_BOOT_DIR} && \
			printf "Copy tiboot3.bin... \n" || printf "Fails to copy tiboot3.bin... \n"
	else
		cp ${SPL_ORI} ${UBOOT_ORI} ${MOUNT_BOOT_DIR} && \
			printf "Copy %s and %s ... \n" ${SPL_ORI} ${UBOOT_ORI} || printf "Fails to copy %s and %s ... \n" ${SPL_ORI} ${UBOOT_ORI}
	fi
	sudo umount -q ${DRIVE}?
	sleep 0.1
	[ -d $MOUNT_BOOT_DIR ] && sudo rm -rf ${MOUNT_BOOT_DIR}
}

usage()
{
    echo -e "\nUsage: install_uboot_am6x.sh
    Optional parameters: [-d disk-path] [-s SOC_name] [-c] [-h]"
	echo "
    * This script is used to download required firmware files, generate and copy bootable u-boot binary
    *
    * [-d disk-path]: specify the disk to copy u-boot binary, e.g., /dev/sdd
    * [-s soc_name]: specify the name of SOC
    * [-r]: install for R5
    * [-c]: clean temporary directory
    * [-h]: help

    For example:

    AM62xx:
    * AXON-AM62XX for R5:
    ./install_uboot_am6x.sh -r -s am62xx -d /dev/sdX
    * AXON-AM62XX for A53:
    ./install_uboot_am6x.sh -s am62xx -d /dev/sdX

    AM62Ax:
    * AXON-AM62AX for R5:
    ./install_uboot_am6x.sh -r -s am62ax -d /dev/sdX
    * AXON-AM62AX for A53:
    ./install_uboot_am6x.sh -s am62ax -d /dev/sdX
"
}

print_settings()
{
	echo "*************************************************************"
	echo "Before run this script, please build u-boot first!
	"
	echo "The disk path to flash u-boot: $DRIVE"
	echo "Make target: ${PLATFORM}"
	echo "*************************************************************

	"
}

if [ $# -eq 0 ]; then
	usage
	exit 1
fi

while getopts "rchd:s:" OPTION
do
    case $OPTION in
        d)
           DRIVE="$OPTARG"
           ;;
        s)
           SOC="$OPTARG"
           ;;
        r)
           FOR_R5=1
           ;;
		c)
		   rm -rf ${FIRMWARE_DIR} ${K3IG_DIR} ${ATF_DIR} ${OPTEE_DIR}
		   echo "Clean ${FIRMWARE_DIR}, ${K3IG_DIR}, ${ATF_DIR} and ${OPTEE_DIR}..."
		   exit
		   ;;
        ?|h) usage
           exit
           ;;
    esac
done

if [ "$(id -u)" = "0" ]; then
   echo "This script can not be run as root"
   exit 1
fi

setup_platform
print_settings
install_firmware
generate_uboot_binary
copy_uboot_binary

