#!/bin/sh

IMX8_BOARD_NAME=
IMX8_MODEL=

apalis_mount_boot() {
	mkdir -p /boot
	mount -t jffs2 mtd:rootfs_data /boot
}

imx8_board_detect() {
	local machine
	local name

	machine=$(cat /proc/device-tree/model)

	case "$machine" in
	"Freescale i.MX8QXP MEK")
		name="ubox"
		machine="MEITUAN01-5G"
		;;

	*)
		name="generic"
		;;
	esac

	[ -z "$IMX8_BOARD_NAME" ] && IMX8_BOARD_NAME="$name"
	[ -z "$IMX8_MODEL" ] && IMX8_MODEL="$machine"

	[ -e "/tmp/sysinfo/" ] || mkdir -p "/tmp/sysinfo/"

	echo "$IMX8_BOARD_NAME" > /tmp/sysinfo/board_name
	echo "$IMX8_MODEL" > /tmp/sysinfo/model
}
