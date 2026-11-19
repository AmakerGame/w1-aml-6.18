# Installation

This guide installs the prebuilt W1 Wi-Fi modules from a GitHub release. The
modules are only valid for the exact kernel release they were built against.

## Requirements

- AArch64 Linux system
- Kernel release matching the release assets (currently `6.18.59-meson64`)
- `cfg80211`, `mac80211`, and SDIO support enabled in the kernel
- A device-tree `amlogic,aml-wifi` node for boards that use the GPIO IRQ path
- Firmware/calibration files under `/lib/firmware/aml/`

## 1. Download the release assets

Open the latest release:

<https://github.com/AmakerGame/w1-aml-6.18/releases>

Download:

- `aml_sdio.ko`
- `vlsicomm.ko`
- `aml_wifi_firmware.tar.gz`
- `sha256sums.txt`

Verify the files before installing:

```sh
sha256sum -c sha256sums.txt
```

Only continue if all checksums match.

## 2. Check kernel compatibility

```sh
uname -r
modinfo aml_sdio.ko | grep vermagic
modinfo vlsicomm.ko | grep vermagic
```

The `vermagic` value must match `uname -r`. For the current verified build this
is `6.18.59-meson64 SMP preempt mod_unload aarch64`.

## 3. Install firmware/calibration files

```sh
sudo mkdir -p /lib/firmware/aml
sudo tar -xzf aml_wifi_firmware.tar.gz -C /lib/firmware/aml
ls /lib/firmware/aml
```

The driver requests paths like `aml/aml_wifi_rf_0125.txt`, so the files must end
up directly inside `/lib/firmware/aml/`.

## 4. Install modules

```sh
KVER=$(uname -r)
sudo install -Dm0644 aml_sdio.ko /lib/modules/$KVER/updates/
sudo install -Dm0644 vlsicomm.ko /lib/modules/$KVER/updates/
sudo depmod -a
```

If `updates/` does not exist on your distribution, use
`/lib/modules/$KVER/kernel/drivers/net/wireless/` or another directory already
scanned by `depmod`.

## 5. Load the modules

Preferred:

```sh
sudo modprobe cfg80211 mac80211
sudo modprobe aml_sdio
sudo modprobe vlsicomm
```

Manual alternative:

```sh
sudo insmod ./aml_sdio.ko
sudo insmod ./vlsicomm.ko
```

Load order matters: `aml_sdio` must be present before `vlsicomm` initializes the
SDIO HAL.

## 6. Verify the driver

```sh
ip link show
iw dev
lsmod | grep -E 'vlsicomm|aml_sdio'
sudo dmesg | grep -Ei 'wifi|wlan|aml|sdio' | tail -100
```

Expected output includes `wlan0` and usually `p2p0`. Then check Wi-Fi with the
desktop environment or `wpa_supplicant`/`NetworkManager`.

Quick checks:

```sh
sudo iw dev wlan0 scan | head -40
nmcli device status
nmcli device wifi list
```

## Persistence

To load the modules at boot, create `/etc/modules-load.d/w1-aml.conf`:

```text
cfg80211
mac80211
aml_sdio
vlsicomm
```

`cfg80211` and `mac80211` are usually loaded automatically by other Wi-Fi
modules or NetworkManager. Keeping them listed explicitly is harmless.

## Unloading

Do not rely on `rmmod` for normal operation. The driver intentionally pins its
module refcount because several timers/workqueues are shared between the SDIO
transport and the fullmac driver; unloading caused use-after-free panics during
testing. Reboot to unload the driver.

## Troubleshooting

### `modprobe: FATAL: Module vlsicomm not found`

- Run `sudo depmod -a` after copying the `.ko` files.
- Confirm the files are under `/lib/modules/$(uname -r)/`.
- Confirm `uname -r` matches the `vermagic` string in `modinfo`.

### Firmware errors in `dmesg`

Look for messages about `aml/aml_wifi_rf` or `request_firmware`. Make sure the
contents of `aml_wifi_firmware.tar.gz` were extracted into
`/lib/firmware/aml/`, not `/lib/firmware/` directly.

### `wlan0` does not appear

- Check `sudo dmesg | grep -Ei 'wifi_dt|aml-wifi|w1|sdio'`.
- Confirm the DT node exists: `ls /proc/device-tree/aml_wifi` or inspect
  `/proc/device-tree` with `dtc`.
- Confirm the SDIO device is visible: `lspci -nn` is not useful for SDIO; use
  `dmesg | grep -i sdio` and board-specific SDIO diagnostics.
- If `wifi_dt` reports no usable IRQ, fix the `amlogic,aml-wifi` node. See
  [device-tree.md](device-tree.md).

### Scans stay stuck or no scan results

This was the original symptom that motivated the GPIO IRQ path. Make sure the
running kernel was built with `USE_GPIO_IRQ=1` / `USE_SDIO_IRQ=0`, the DT node is
present, and the GPIO IRQ maps to the same interrupt used by the vendor Android
stack.

### Bluetooth

Bluetooth on the same W1 module is not implemented by this Wi-Fi driver. Use the
separate [AmakerGame/aml_bt](https://github.com/AmakerGame/aml_bt) project for
the companion Bluetooth stack.
