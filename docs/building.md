# Building from source

The driver is an out-of-tree kernel module. It must be built against the same
kernel source/configuration that will load it on the target board.

## Requirements

- Linux kernel source tree for the target kernel (for example `6.18.y`)
- Kernel prepared for module builds:

  ```sh
  make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig
  make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- olddefconfig
  make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules_prepare
  make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules
  ```

  At minimum the build needs `Module.symvers` and generated headers. If you are
  building against a distro/kernel package, use the matching kernel headers or
  source tree.

- `aarch64-linux-gnu-` toolchain (or the toolchain used by your kernel build)
- `perl` for `create_version_file.pl`
- `make`, `gcc`, `bc`, `flex`, `bison`, and normal kernel build dependencies

## Build command

From the repository root:

```sh
make -C /path/to/kernel \
     M=$PWD/project_w1/vmac \
     ARCH=arm64 \
     CROSS_COMPILE=aarch64-linux-gnu- \
     modules
```

If your kernel tree already exports `ARCH`/`CROSS_COMPILE`, you can omit them;
the Makefile defaults to `arm64`/`aarch64-linux-gnu-` when `ARCH` is unset.

## Output

The build produces:

```text
project_w1/vmac/aml_sdio.ko
project_w1/vmac/vlsicomm.ko
```

Check the result before installing:

```sh
modinfo project_w1/vmac/aml_sdio.ko | egrep 'vermagic|depends'
modinfo project_w1/vmac/vlsicomm.ko | egrep 'vermagic|depends'
```

`vermagic` must match the target kernel's `uname -r`.

## Clean

```sh
make -C /path/to/kernel M=$PWD/project_w1/vmac clean
```

This removes generated objects, `*.ko`, `*.mod.c`, `Module.symvers`, and
`print_version.c` inside `project_w1/vmac`.

## Build notes

- `project_w1/vmac/Makefile` builds two modules:
  - `aml_sdio.ko` from `w1_sdio/w1_sdio.o` plus `wifi_dt.o`
  - `vlsicomm.ko` from the fullmac Wi-Fi sources
- `wifi_dt.o` is deliberately kept out of `vlsicomm` to avoid duplicate symbol
  exports.
- Generated `*.mod.c` files are filtered out of `SRC_FILE`; otherwise
  `aml_sdio.mod.c` can be linked into `vlsicomm` and cause duplicate `w1_*`
  symbol errors.
- `print_version.c` is regenerated automatically by the Makefile.
- Current defaults use the GPIO IRQ path:

  ```make
  EXTRA_CFLAGS += -DUSE_GPIO_IRQ=1
  EXTRA_CFLAGS += -DUSE_SDIO_IRQ=0
  ```

  Do not switch this back to SDIO in-band IRQ on Meson SC2 unless you have
  verified that path on the target hardware; it was unreliable for this port.

## Installing a locally built module

Use the same steps as [installation.md](installation.md), but copy the `.ko`
files from `project_w1/vmac/` instead of downloading release assets.
