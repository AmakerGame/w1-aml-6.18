# Device-tree requirements

The W1 driver port uses `USE_GPIO_IRQ=1` and `USE_SDIO_IRQ=0`. The host
interrupt is provided by the `wifi_dt.c` platform glue, which binds to an
`amlogic,aml-wifi` device-tree node and exports the IRQ number/trigger level to
the rest of the driver.

This is required on Meson SC2 / S905X4 hardware where the SDIO in-band interrupt
did not reliably deliver scan-completion and RX events.

## Required properties

| Property | Required | Purpose |
|---|---:|---|
| `compatible` | yes | Must be `"amlogic,aml-wifi"` or `"amlogic, aml-wifi"`. |
| `status` | yes | Use `"okay"` to enable the node. |
| `irq_trigger_type` | recommended | One of `GPIO_IRQ_LOW`, `GPIO_IRQ_HIGH`, `GPIO_IRQ_RISING`, `GPIO_IRQ_FALLING`. |
| `interrupts-extended` | preferred | Interrupt specifier already routed to the SoC GPIO interrupt controller. |
| `interrupt-gpios` | fallback | GPIO specifier used with `gpio_to_irq()` when `interrupts-extended` is absent. |

`power_on-gpios`, `reset-gpios`, PWM properties, and similar fields may exist in
vendor device trees for board-specific power sequencing. The current `wifi_dt.c`
glue consumes the interrupt properties above; board power/reset handling should
be provided by the board's normal regulator/GPIO/pinctrl descriptions unless the
board vendor driver requires otherwise.

## Preferred form: `interrupts-extended`

Use this form when the Wi-Fi IRQ is wired to the Meson GPIO interrupt
controller. The exact cell values are SoC/board-specific; copy the working
values from the vendor DT or from `/proc/device-tree`/`running.dts` on the
target board.

Conceptual example:

```dts
aml_wifi: aml_wifi {
    compatible = "amlogic,aml-wifi";
    status = "okay";
    irq_trigger_type = "GPIO_IRQ_LOW";
    interrupts-extended = <&gpio_intc PAD TYPE>;
};
```

On Meson parts the GPIO interrupt controller is commonly referenced as
`gpio_intc`; `PAD` is the GPIO pad number and `TYPE` is the irqchip trigger
type expected by that controller.

## Fallback form: `interrupt-gpios`

If `irq_of_parse_and_map()` cannot map `interrupts-extended`, the driver falls
back to `interrupt-gpios` and calls `gpio_to_irq()`:

```dts
aml_wifi: aml_wifi {
    compatible = "amlogic,aml-wifi";
    status = "okay";
    irq_trigger_type = "GPIO_IRQ_LOW";
    interrupt-gpios = <&gpio GPIOX_7 GPIO_ACTIVE_LOW>;
};
```

The fallback only works when the GPIO controller has an irqchip that can
translate the GPIO to an IRQ. On boards where it cannot, `wifi_dt` logs
`no usable wifi irq` and Wi-Fi scanning/operation will not work correctly.

## Trigger levels

`irq_trigger_type` is parsed by `wifi_dt.c` and passed to the HAL. Supported
values:

| DT string | Meaning |
|---|---|
| `GPIO_IRQ_LOW` | Low-level interrupt |
| `GPIO_IRQ_HIGH` | High-level interrupt |
| `GPIO_IRQ_RISING` | Rising edge |
| `GPIO_IRQ_FALLING` | Falling edge |

Most Amlogic TV-box Wi-Fi modules use a low-level GPIO interrupt. Check the
vendor Android DT or logic analyzer/oscilloscope readings if association or
scanning is unreliable.

## X96 Max Plus Ultra reference

The X96 Max Plus Ultra SC2 board ships an `aml_wifi` node with
`compatible = "amlogic,aml-wifi"`, `status = "okay"`,
`irq_trigger_type = "GPIO_IRQ_LOW"`, an `interrupt-gpios` entry, and an
`interrupts-extended` entry routed to the Meson GPIO interrupt controller.

The shipped DTS uses numeric phandles/pad values. Use the values from the
board's working DT or `running.dts`; do not copy GPIO numbers from another
board.

## Verification

After booting with the driver installed:

```sh
ls /proc/device-tree/aml_wifi
sudo dmesg | grep -Ei 'wifi_dt|aml-wifi|WIFI_INT'
```

Healthy output should include a `wifi_dt` message such as:

```text
wifi_dt: gpio-intc irq <N> mapped
```

or:

```text
wifi_dt: gpio <N> -> irq <M>
```

If the node is missing or malformed, `aml_sdio` still loads but logs
`wifi_dt_register failed` or `no usable wifi irq`; scans may remain stuck.
