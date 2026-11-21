# w1-aml-6.18

Port of the Amlogic **W1** Wi-Fi driver family to **Linux 6.18**.

This repository is based on [CoreELEC/w1-aml](https://github.com/CoreELEC/w1-aml)
(branch `v1.8.3.3_20260310-w1`) and contains the changes needed to build and run
the driver on modern `arm64` kernels. It has been verified on real hardware with
kernel `6.18.59-meson64`.

## Hardware

| Item | Details |
|---|---|
| Chip family | Amlogic W150S1 / W155S1 ("W1") |
| Bus | SDIO 3.0 |
| Wireless | Wi-Fi 802.11ac 1T1R fullmac |
| Combo function | Bluetooth is present on the same module but is handled by a separate driver |
| SDIO IDs | `8888:8888` |
| Verified board | X96 Max Plus Ultra (Amlogic S905X4 / Meson SC2) |
| Verified module | CDTECH CDW.47W3155-00 |
| Verified kernel | `6.18.59-meson64` |

## What is included

The build produces two kernel modules:

| Module | Purpose |
|---|---|
| `aml_sdio.ko` | SDIO transport, probe, TX/RX rings, interrupt handling |
| `vlsicomm.ko` | Main fullmac Wi-Fi driver; registers `wlan0` and `p2p0` through cfg80211 |

Typical load order:

```sh
modprobe cfg80211 mac80211
modprobe aml_sdio
modprobe vlsicomm
```

`vlsicomm` depends on `aml_sdio`, so `modprobe vlsicomm` will normally pull in
the transport module automatically.

## Quick start with prebuilt modules

Prebuilt `arm64` modules are published in
[Releases](https://github.com/AmakerGame/w1-aml-6.18/releases). The current
verified build targets kernel release `6.18.59-meson64`.

The `.ko` files are kernel-release specific. Check before installing:

```sh
uname -r
modinfo aml_sdio.ko | grep vermagic
```

The `vermagic` string must match `uname -r`.

### 1. Install firmware/calibration files

```sh
sudo mkdir -p /lib/firmware/aml
sudo tar -xzf aml_wifi_firmware.tar.gz -C /lib/firmware/aml
```

The driver requests files such as `aml/aml_wifi_rf_XXXX.txt` through the kernel
firmware loader, so the extracted files must be visible under
`/lib/firmware/aml/`.

### 2. Install the modules

```sh
KVER=$(uname -r)
sudo install -Dm0644 aml_sdio.ko  /lib/modules/$KVER/updates/
sudo install -Dm0644 vlsicomm.ko  /lib/modules/$KVER/updates/
sudo depmod -a
```

### 3. Load the driver

```sh
sudo modprobe cfg80211 mac80211
sudo modprobe aml_sdio
sudo modprobe vlsicomm
```

### 4. Verify

```sh
ip link show
iw dev
sudo dmesg | grep -Ei 'wifi|wlan|aml|sdio' | tail -80
```

Expected result: `wlan0` and usually `p2p0` appear, and NetworkManager or
`wpa_supplicant` can scan/connect normally.

For a detailed installation and troubleshooting guide, see
[docs/installation.md](docs/installation.md).

## Device tree requirement

This port is built with `USE_GPIO_IRQ=1` and `USE_SDIO_IRQ=0`. On Meson SC2 the
SDIO in-band interrupt path did not reliably deliver scan/RX events, so the
driver gets its host interrupt from an `amlogic,aml-wifi` device-tree node
through the SoC GPIO interrupt controller.

A minimal node looks like this; GPIO numbers and interrupt specifiers are
board-specific:

```dts
aml_wifi: aml_wifi {
    compatible = "amlogic,aml-wifi";
    status = "okay";
    irq_trigger_type = "GPIO_IRQ_LOW";

    /* Preferred: interrupt already routed to the GPIO irqchip. */
    interrupts-extended = <&gpio_intc WIFI_IRQ_PAD IRQ_TYPE>;

    /* Fallback used when the irqchip can translate a GPIO to an IRQ. */
    interrupt-gpios = <&gpio WIFI_IRQ_GPIO GPIO_ACTIVE_LOW>;
};
```

See [docs/device-tree.md](docs/device-tree.md) for property details and
troubleshooting.

## Firmware and calibration

The main Wi-Fi firmware is embedded in `project_w1/common/fucode_em4.h`.
Runtime RF calibration is loaded from `/lib/firmware/aml/`:

- `aml_wifi_rf.txt` — generic fallback
- `aml_wifi_rf_XXXX.txt` — per-module calibration selected by SDIO CIS
- `aml_wifi_rf_iton.txt`, `aml_wifi_rf_ampak.txt`, `aml_wifi_rf_fn_link.txt` —
  module-family fallbacks
- `aml_wifi_drv_cfg_0.conf` — driver configuration file

Release asset `aml_wifi_firmware.tar.gz` contains these files.

## Build from source

The driver is built out-of-tree against a prepared kernel tree. See
[docs/building.md](docs/building.md) for the full command and requirements.

Short version:

```sh
make -C /path/to/kernel \
     M=$PWD/project_w1/vmac \
     ARCH=arm64 \
     CROSS_COMPILE=aarch64-linux-gnu- \
     modules
```

The Makefile generates `print_version.c`, propagates the driver's configuration
flags through `ccflags-y`, and filters generated `*.mod.c` files out of the
object list.

## Porting notes for Linux 6.18

Compared with the upstream CoreELEC tree, this repository includes:

- Modern cfg80211 op signatures (`wireless_dev`, `link_id`, NL80211 key types,
  antenna/bitrate-mask callbacks, `cfg80211_new_sta`/`cfg80211_del_sta`).
- Timer API updates for kernels after `from_timer`/`timer_setup` changes.
- Driver-local SHA-256/HMAC helpers renamed with a `w1_` prefix to avoid
  collisions with kernel crypto headers.
- GPIO-line IRQ support through `wifi_dt.c` and the `amlogic,aml-wifi` DT node.
- Platform stubs for board callbacks that do not exist on mainline kernels.
- Module symbol/export fixes and a `Makefile` fix that prevents
  `aml_sdio.mod.c` from being linked into `vlsicomm`.
- Runtime stability fixes for RX-path station lifetime, TX-drop TID bounds,
  module refcount pinning, and cfg80211 NULL VIF guards.
- Debug output stays at `KERN_DEBUG`: `opt_all.h` no longer remaps
  `pr_debug` to `pr_info`, so `AML_OUTPUT`/`DPRINTF` messages are silent
  by default (console loglevel and dynamic debug can still control them)
  instead of flooding `dmesg`/journald as info-level spam.

## Runtime notes

- Both modules intentionally pin their module refcount. `rmmod` is not a safe
  unload path; reboot the board to unload the driver.
- Load order matters: transport first (`aml_sdio`), then the fullmac driver
  (`vlsicomm`).
- Interfaces are `wlan0` (STA) and `p2p0` (P2P). Band and role handling is done
  through cfg80211/mac80211 and standard Linux networking tools.
- Bluetooth on the same W1 module is not part of this Wi-Fi driver; see
  [AmakerGame/aml_bt](https://github.com/AmakerGame/aml_bt) for the companion
  Bluetooth work.

## Verified status

Verified on X96 Max Plus Ultra / `6.18.59-meson64`:

- `aml_sdio.ko` + `vlsicomm.ko` load and register `wlan0`/`p2p0`.
- Scanning works on 2.4 GHz and 5 GHz.
- Association, DHCP, and internet connectivity were verified.
- Bluetooth coexistence was verified with the separate `aml_bt` stack.
- Kernel log volume was reduced for normal operation.

## License

Upstream `CoreELEC/w1-aml` does not ship a `LICENSE` file. Source files carry
vendor/Amlogic copyright headers and some code is derived from Broadcom fullmac
code. This port adds no new licensing claims; use it at your own discretion.
