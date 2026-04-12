/*
 * ael_bootloader.h — DEPRECATED shim, use ael_usb.h instead.
 *
 * Kept for source compatibility. New firmwares should include ael_usb.h
 * and call ael_usb_init() + poll ael_bl_flag directly.
 */
#pragma once
#include "ael_usb.h"

/* No-op macro — bootloader detection is handled by ael_usb.c callback */
#define AEL_BOOTLOADER_THREAD_DEFINE()
