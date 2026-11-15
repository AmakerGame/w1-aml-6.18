# w1-aml — Amlogic W1 (W150S1/W155S1) Wi-Fi driver for Linux 6.18

Port of the Amlogic **W1** series fullmac Wi-Fi driver to modern mainline
kernels. Originally written for vendor kernels (5.4 / 5.15), this tree has been
forward-ported to **Linux 6.18** and verified on real hardware.

## Upstream sources

- **Primary upstream:** [CoreELEC/w1-aml](https://github.com/CoreELEC/w1-aml)
  (branch `v1.8.3.3_20260310-w1`)
- **Original source:** [khadas/wifi-amlogic-w1](https://github.com/khadas/wifi-amlogic-w1)
  (Khadas Vim3 / VIM1 boards)

This repository contains the CoreELEC tree plus local forward-port changes
listed below. Git history of the upstream clone is preserved.

## Hardware

- Amlogic **W150S1 / W155S1** (a.k.a. "W1") — SDIO 3.0 Wi-Fi 802.11ac 1T1R
  + Bluetooth combo module
- SDIO IDs: `8888:8888` (fake/virtual IDs burned into the module)
- Verified on: **X96 Max Plus Ultra** (Amlogic S905X4 / Meson SC2),
  module CDTECH CDW.47W3155-00, kernel `6.18.59-meson64`

## Produced modules

| Module | Purpose |
|---|---|
| `aml_sdio.ko` | SDIO transport layer (probe, TX/RX rings, IRQ) |
| `vlsicomm.ko` | Main fullmac driver, registers `wlan0`/`p2p0` via cfg80211 |

Load order: `cfg80211` -> `mac80211` -> `aml_sdio` -> `vlsicomm`.

## Firmware / calibration

The chip firmware is embedded (`common/fucode_em4.h`). RF calibration is
loaded at runtime from `/lib/firmware/aml/`:

- `aml_wifi_rf.txt` (generic fallback)
- `aml_wifi_rf_XXXX.txt` (per-module variants, selected by SDIO CIS)
- `aml_wifi_drv_cfg_0.conf`

Copy the `*.txt` files from `project_w1/vmac/` into `/lib/firmware/aml/`
on the target system.

## Build

Requires a kernel tree with prepared headers and `Module.symvers`
(e.g. `make modules_prepare` + full `make modules` once).

```sh
make -C /path/to/kernel M=$PWD/project_w1/vmac \
     ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules
```

The Makefile auto-generates `print_version.c` and propagates the driver's
configuration flags (`CONFIG_SDIO_IF`, `WITH_RF`, `CONFIG_AML_CFG80211`, ...)
through `ccflags-y`.

## Forward-port changes for 6.18 (vs upstream CoreELEC tree)

- **cfg80211 ops modernized** (~20 callbacks in `wifi_cfg80211.c`):
  `struct net_device *` -> `struct wireless_dev *`, `link_id` parameters,
  `enum nl80211_key_type` instead of `bool pairwise`, `radio_idx`,
  `u64` mgmt-TX/remain-on-channel cookies, `cfg80211_new_sta/del_sta`
  on `wdev`, `set_antenna`/`get_antenna`/`set_bitrate_mask` signatures.
- **Timers:** `from_timer` -> `timer_container_of` (removed in 6.15+),
  `del_timer`/`hrtimer` API adjustments.
- **Crypto:** driver-local SHA-256/HMAC helpers renamed (`w1_*`) to avoid
  collisions with `crypto/sha2.h`.
- **IRQ:** switched from vendor GPIO-IRQ path to standard **SDIO in-band
  interrupt** (`USE_GPIO_IRQ=0`, `USE_SDIO_IRQ=1`) — no Amlogic GPIO
  platform APIs required.
- **Platform stubs:** weak/default implementations for
  `set_usb_wifi_power()` / `wifi_get_mac()` (vendor board callbacks absent
  on mainline).
- **Symbol hygiene:** `MODULE_IMPORT_NS` quoting fixed, duplicate
  `import_ns` cleaned, `MODULE_DESCRIPTION()` added.
- **Build fix:** `SRC_FILE` now filters out generated `*.mod.c` files —
  previously `aml_sdio.mod.c` was swept into `vlsicomm` objects, causing
  duplicate `w1_*` symbol exports at insmod time.
- Kernel API shim for `ieee80211_chandef_rate_flags` where needed.

## License

Upstream `CoreELEC/w1-aml` ships **no LICENSE file**; sources carry vendor
copyright headers (Amlogic, and Broadcom-derived fullmac code). This port
inherits the same status — no additional licensing claims are made.
Kernel-module derivative of vendor code; use at your own discretion.

## Status / known issues

- Wi-Fi interface up, scanning and cfg80211 registration verified on
  X96 Max Plus Ultra / 6.18.59-meson64.
- `p2p0` virtual interface is created (P2P support compiled in).
- Bluetooth over the same chip: not yet ported (separate `hci` path).
- Modules are built against a specific kernel release; vermagic must match
  `uname -r` (CONFIG_MODVERSIONS is off in the target kernel).
