# nRF52840 UF2 Bootloader & Firmware — How It Works

*Target board: nice!nano v1 (Adafruit UF2 Bootloader 0.6.0, nRF52840)*

---

## Overview

When you plug in a nice!nano, two separate programs are involved: the **UF2 bootloader** (factory-installed, permanent) and your **application firmware** (what you flash). Understanding how they coexist and hand off control is essential for reliable flashing and recovery.

---

## Flash Memory Layout

The nRF52840 has 1 MB of flash. The bootloader and the application live at different offsets and never overlap:

```
nRF52840 Flash (1 MB total)
┌───────────────────────────────────────────┐  0x00000000
│         Adafruit UF2 Bootloader           │
│         (factory-installed, read-only)    │
│         ~4 KB, starts at reset vector     │
├───────────────────────────────────────────┤  0x00001000
│         Application Firmware              │
│         (what you flash via UF2)          │
│         up to 0xEE000 bytes (~952 KB)     │
└───────────────────────────────────────────┘  0x000FFFFF
```

The bootloader occupies the first 4 KB. Every application must be built with `FLASH_LOAD_OFFSET=0x1000` so its reset vector lands at the correct address.

In Zephyr this is expressed in `prj.conf`:
```
CONFIG_FLASH_LOAD_OFFSET=0x1000
CONFIG_FLASH_LOAD_SIZE=0xEE000
```

And in `app.overlay` the partition is declared explicitly so Zephyr links against the right region:
```dts
&flash0 {
    partitions {
        nicenano_app: partition@1000 {
            reg = <0x00001000 0x000EE000>;
        };
    };
};
```

---

## Boot Sequence

Every time the board powers on or resets, execution starts at address `0x00000000` — the bootloader:

```
Power-on / Reset
       │
       ▼
 ┌─────────────────────────────────────────┐
 │         UF2 Bootloader runs first       │
 │                                         │
 │  1. Check GPREGRET register             │
 │     0x40000000 + 0x51C (Power domain)   │
 │     Value = 0x57?  → enter DFU mode     │
 │     Value = other? → continue           │
 │                                         │
 │  2. Check double-tap RST                │
 │     Two RST pulses within ~500 ms?      │
 │     Yes → enter DFU mode               │
 │                                         │
 │  3. Jump to application at 0x1000      │
 └─────────────────────────────────────────┘
       │                      │
  DFU mode                 App runs
  (NICENANO USB drive      (your firmware)
   appears on host)
```

**GPREGRET** (General Purpose Retention Register) is a register in the nRF52840 Power peripheral that survives a software reset (`NVIC_SystemReset()`). This is the mechanism the firmware uses to ask the bootloader to stay in DFU mode on the next boot.

---

## DFU Mode — How UF2 Flashing Works

When the bootloader enters DFU mode it presents itself as a USB Mass Storage device. On the host you see a drive named **NICENANO** containing three files:

| File | Purpose |
|------|---------|
| `CURRENT.UF2` | Readback of currently-flashed application |
| `INDEX.HTM` | Redirects browser to Adafruit documentation |
| `INFO_UF2.TXT` | Board info: family ID, bootloader version |

Flashing is as simple as copying a `.uf2` file onto the drive:

```bash
cp build/zephyr/zephyr.uf2 /media/$USER/NICENANO/
sync
```

The bootloader:
1. Intercepts the FAT write at the filesystem layer
2. Parses each 512-byte UF2 block (256 bytes payload + metadata)
3. Validates the family ID (`0x239A00B3` for nRF52840)
4. Writes the payload to the target flash address
5. After the last block, automatically reboots into the new application

The UF2 block format carries the target flash address directly, so the bootloader always writes each chunk to exactly the right location — no host-side programmer needed.

---

## 1200-Baud Bootloader Re-Entry

Once application firmware is running, the NICENANO drive is gone. How do you get back into DFU mode without physically touching a RST pad?

The answer is the **1200-baud touch protocol**, originally popularized by Arduino. Any USB CDC serial port opened at exactly 1200 baud signals "enter bootloader":

```
Host (Python / Arduino IDE / ael flash driver)
       │
       │  opens /dev/ttyACM0 at baud=1200
       │  USB SET_LINE_CODING(dwDTERate=1200)
       ▼
Firmware (_usbd_msg_cb callback)
       │
       │  msg->type == USBD_MSG_CDC_ACM_LINE_CODING
       │  uart_line_ctrl_get(..., UART_LINE_CTRL_BAUD_RATE) == 1200
       ▼
       │  atomic_set(&_enter_bl_flag, 1)
       │
main loop detects flag
       │
       │  k_msleep(50)        — let host close the port
       │  GPREGRET = 0x57    — mark "stay in DFU" for bootloader
       │  NVIC_SystemReset() — software reset
       ▼
Bootloader sees GPREGRET=0x57 → enters DFU mode
NICENANO drive reappears on host
```

### Why This Needs `CONFIG_UART_LINE_CTRL=y`

The Zephyr CDC-ACM driver exposes line coding information through a runtime API gated behind this Kconfig option. Without it, `uart_line_ctrl_get()` returns `-ENOTSUP` regardless of what baud rate the host requested.

Required entries in `prj.conf`:
```
CONFIG_USB_DEVICE_STACK_NEXT=y
CONFIG_USBD_CDC_ACM_CLASS=y
CONFIG_UART_LINE_CTRL=y          # ← required for uart_line_ctrl_get()
```

### Why the Callback Approach Is Reliable

An earlier implementation polled `uart_line_ctrl_get()` every 200 ms from a background thread. This missed the 1200-baud event because:

- Python's `serial.Serial(port, 1200)` holds the baud rate only for as long as the port is open
- The 200 ms poll interval could easily fall between "open" and "close"

The callback approach (`usbd_msg_register_cb`) fires synchronously on every `SET_LINE_CODING` USB control request, with zero polling latency. The baud rate is read immediately inside the callback and cannot be missed.

---

## Full USB Initialization in Application Code

Because the nice!nano has no dedicated debug/programmer interface (SWD not connected, no built-in UART), having a functional USB CDC port is not optional — it is the only recovery path. If the firmware boots without USB, the board becomes unrecoverable without physical access to the RST pad.

The application initializes the new Zephyr USB stack manually (not via `sample_usbd.h`, to avoid sample-tree Kconfig dependencies):

```c
/* 1. Declare the USB device */
USBD_DEVICE_DEFINE(ael_usbd,
                   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
                   0x239A, 0x000F);   /* Adafruit VID, AEL PID */

/* 2. Declare string descriptors */
USBD_DESC_LANG_DEFINE(ael_lang);
USBD_DESC_MANUFACTURER_DEFINE(ael_mfr, "Adafruit Industries LLC");
USBD_DESC_PRODUCT_DEFINE(ael_product, "AEL nRF52840 Blinky");

/* 3. Declare a Full-Speed configuration */
USBD_DESC_CONFIG_DEFINE(ael_fs_cfg_desc, "FS Configuration");
USBD_CONFIGURATION_DEFINE(ael_fs_config,
                           USB_SCD_SELF_POWERED, 125, &ael_fs_cfg_desc);

/* 4. Register callback, init, enable */
static int usb_init(void)
{
    usbd_add_descriptor(&ael_usbd, &ael_lang);
    usbd_add_descriptor(&ael_usbd, &ael_mfr);
    usbd_add_descriptor(&ael_usbd, &ael_product);
    usbd_add_configuration(&ael_usbd, USBD_SPEED_FS, &ael_fs_config);
    usbd_register_all_classes(&ael_usbd, USBD_SPEED_FS, 1, NULL);
    usbd_device_set_code_triple(&ael_usbd, USBD_SPEED_FS,
                                USB_BCC_MISCELLANEOUS, 0x02, 0x01);
    usbd_msg_register_cb(&ael_usbd, _usbd_msg_cb);  /* ← key line */
    usbd_init(&ael_usbd);
    if (!usbd_can_detect_vbus(&ael_usbd)) usbd_enable(&ael_usbd);
    return 0;
}
```

The nRF52840 can detect VBUS, so `usbd_enable()` is called from the callback (`USBD_MSG_VBUS_READY`) rather than unconditionally from `usb_init()`.

---

## VID/PID Assignments

| State | VID:PID | Host sees |
|-------|---------|-----------|
| UF2 bootloader | `239A:00B3` | `Adafruit nice!nano` |
| Our blinky firmware | `239A:000F` | `AEL nRF52840 Blinky` |

The `239A` vendor ID belongs to Adafruit Industries. Using it for development firmware on Adafruit hardware is acceptable; for production use you would register your own VID.

---

## Recovery Without a RST Button

If firmware is accidentally flashed without a USB CDC stack (or the CDC init fails), the board becomes unreachable over software. Recovery options in order of preference:

1. **1200-baud touch** (requires working CDC in firmware) — automated, no physical access
2. **Double-tap RST pad** — two quick contacts of the RST pad to GND within ~500 ms
3. **Single RST to GND** — enters app if GPREGRET ≠ 0x57
4. **Power cycle** — same as single RST

The RST pad on the nice!nano v1 is a small exposed copper pad on the underside of the PCB near the USB connector. A bent wire or tweezers can reach it.

**Lesson learned:** Always include a working USB CDC stack in every firmware image flashed to this board, even during bringup. The 1200-baud re-entry is the only automated recovery path.

---

## Summary

| Concept | Detail |
|---------|--------|
| Bootloader location | `0x00000000`, always runs first on reset |
| App location | `0x00001000` (set via `FLASH_LOAD_OFFSET`) |
| DFU trigger (software) | Write `GPREGRET=0x57`, call `NVIC_SystemReset()` |
| DFU trigger (hardware) | Double-tap RST pad within 500 ms |
| 1200-baud detection | `USBD_MSG_CDC_ACM_LINE_CODING` callback + `uart_line_ctrl_get()` |
| Required Kconfig | `CONFIG_UART_LINE_CTRL=y` |
| UF2 family ID | `0x239A00B3` (nRF52840, Adafruit bootloader) |
| NICENANO drive path | `/media/$USER/NICENANO` on Linux |
