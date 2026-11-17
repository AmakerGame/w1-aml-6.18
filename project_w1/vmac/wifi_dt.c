// SPDX-License-Identifier: GPL-2.0
/*
 * Minimal platform glue for the Amlogic W1 SDIO Wi-Fi chip on
 * mainline-style kernels.
 *
 * The vendor driver expects two functions normally provided by
 * Amlogic's out-of-tree wifi_dt.c:
 *     int wifi_irq_num(void);
 *     int wifi_irq_trigger_level(void);
 *
 * This file implements them by parsing the "amlogic,aml-wifi"
 * device-tree node.  The Wi-Fi IRQ is routed through the SoC
 * GPIO interrupt controller (amlogic,meson-*-gpio-intc), so the
 * DT node must provide the mapping via "interrupts-extended"
 * (preferred) or "interrupt-gpios" + gpio_to_irq() as fallback.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/string.h>

/* WIFI_GPIO_IRQ_* values, see wifi_hal_com.h */
#define WIFI_GPIO_IRQ_HIGH	0
#define WIFI_GPIO_IRQ_LOW	1
#define WIFI_GPIO_IRQ_RISING	2
#define WIFI_GPIO_IRQ_FALLING	3

static int wifi_irq_num_val = -1;
static int wifi_trigger_level = WIFI_GPIO_IRQ_LOW;

int wifi_irq_num(void)
{
	return wifi_irq_num_val;
}
EXPORT_SYMBOL(wifi_irq_num);

int wifi_irq_trigger_level(void)
{
	return wifi_trigger_level;
}
EXPORT_SYMBOL(wifi_irq_trigger_level);

static int wifi_dt_parse_trigger(struct device_node *np)
{
	const char *trigger = NULL;

	if (of_property_read_string(np, "irq_trigger_type", &trigger))
		return 0;

	if (!strcmp(trigger, "GPIO_IRQ_LOW"))
		wifi_trigger_level = WIFI_GPIO_IRQ_LOW;
	else if (!strcmp(trigger, "GPIO_IRQ_HIGH"))
		wifi_trigger_level = WIFI_GPIO_IRQ_HIGH;
	else if (!strcmp(trigger, "GPIO_IRQ_RISING"))
		wifi_trigger_level = WIFI_GPIO_IRQ_RISING;
	else if (!strcmp(trigger, "GPIO_IRQ_FALLING"))
		wifi_trigger_level = WIFI_GPIO_IRQ_FALLING;
	else
		pr_warn("wifi_dt: unknown irq_trigger_type %s\n", trigger);

	return 0;
}

static int wifi_dt_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int irq, gpio;

	wifi_dt_parse_trigger(np);

	/* Preferred path: interrupt spec routed to the meson gpio-intc
	 * (interrupts-extended = <&gpio_intc PAD TYPE>). */
	irq = irq_of_parse_and_map(np, 0);
	if (irq > 0) {
		dev_info(&pdev->dev, "wifi_dt: gpio-intc irq %d mapped\n", irq);
		wifi_irq_num_val = irq;
		return 0;
	}

	/* Fallback: interrupt-gpios + gpio_to_irq() (needs an irqchip
	 * on the gpio controller). */
	gpio = of_get_named_gpio(np, "interrupt-gpios", 0);
	if (gpio_is_valid(gpio)) {
		irq = gpio_to_irq(gpio);
		if (irq > 0) {
			dev_info(&pdev->dev,
				 "wifi_dt: gpio %d -> irq %d\n", gpio, irq);
			wifi_irq_num_val = irq;
			return 0;
		}
	}

	dev_err(&pdev->dev,
		"wifi_dt: no usable wifi irq (irq_of_parse_and_map=%d gpio=%d)\n",
		irq, gpio);
	return -ENODEV;
}

static void wifi_dt_remove(struct platform_device *pdev)
{
	wifi_irq_num_val = -1;
}

static const struct of_device_id wifi_dt_of_match[] = {
	{ .compatible = "amlogic,aml-wifi" },
	{ .compatible = "amlogic, aml-wifi" },
	{ }
};
MODULE_DEVICE_TABLE(of, wifi_dt_of_match);

static struct platform_driver wifi_dt_driver = {
	.probe	= wifi_dt_probe,
	.remove	= wifi_dt_remove,
	.driver	= {
		.name		= "aml-wifi-dt",
		.of_match_table	= wifi_dt_of_match,
	},
};

int wifi_dt_register(void)
{
	return platform_driver_register(&wifi_dt_driver);
}
EXPORT_SYMBOL(wifi_dt_register);

void wifi_dt_unregister(void)
{
	platform_driver_unregister(&wifi_dt_driver);
}
EXPORT_SYMBOL(wifi_dt_unregister);

MODULE_DESCRIPTION("Amlogic W1 Wi-Fi DT platform glue (GPIO IRQ)");
MODULE_LICENSE("GPL");
