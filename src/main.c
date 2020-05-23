/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <stdio.h>
#include <logging/log.h>
#include <power/reboot.h>
#include <devicetree.h>
#include <drivers/gpio.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 200

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

#if DT_HAS_NODE_STATUS_OKAY(LED0_NODE)
#define LED0 DT_GPIO_LABEL(LED0_NODE, gpios)
#define PIN DT_GPIO_PIN(LED0_NODE, gpios)
#if DT_PHA_HAS_CELL(LED0_NODE, gpios, flags)
#define FLAGS DT_GPIO_FLAGS(LED0_NODE, gpios)
#endif
#else
/* A build error here means your board isn't set up to blink an LED. */
#error "Unsupported board: led0 devicetree alias is not defined"
#define LED0 ""
#define PIN 0
#endif

LOG_MODULE_REGISTER(main);

void get_and_check(struct device **dev, const char *label, u32_t pin, uint32_t flags)
{

	*dev = device_get_binding(label);

	LOG_INF("Configure dev %s: x%04X", label, flags);
	if (*dev == NULL)
	{
		LOG_ERR("Cannot get device %s", label);
		__ASSERT_NO_MSG(0);
		return;
	}

	int32_t ret = gpio_pin_configure(*dev, pin, flags);

	if (ret < 0)
	{
		LOG_ERR("Configuration failed:%d", ret);
		__ASSERT_NO_MSG(0);
		return;
	}
}

#define GPIO_PIN(name) (DT_GPIO_##name##_GPIOS_PIN)
#define GPIO_CONFIGURE(dev, name, flags)                                                          \
	{                                                                                             \
		get_and_check(dev, DT_GPIO_##name##_GPIOS_CONTROLLER, DT_GPIO_##name##_GPIOS_PIN, flags); \
	}
static const int MAX_LOOP_TO_GO = 20000;
void main(void)
{
	LOG_INF("Main started!");

	struct device *dev_led;
	struct device *dev_cap_in;
	struct device *dev_cap_out;

	bool led_is_on = true;

	GPIO_CONFIGURE(&dev_led, LEDS_LED0, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_HIGH | GPIO_OUTPUT);
	GPIO_CONFIGURE(&dev_cap_out, LEDS_CAP_OUT, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_HIGH | GPIO_OUTPUT);
	GPIO_CONFIGURE(&dev_cap_in, KEYS_CAP_IN, GPIO_INPUT | GPIO_ACTIVE_HIGH | GPIO_OUTPUT | GPIO_OPEN_DRAIN);
	//	GPIO_CONFIGURE(&dev_cap_in, KEYS_CAP_IN ,  GPIO_ACTIVE_HIGH| GPIO_OUTPUT  );

	u32_t filtered = 0;
	u32_t mainloop = 0;
	for (;;)
	{
		mainloop += 1;
		gpio_pin_set(dev_led, PIN, (int)led_is_on);
		led_is_on = !led_is_on;
		 k_msleep(SLEEP_TIME_MS);

		// Empty charge
		gpio_pin_set(dev_cap_out, GPIO_PIN(LEDS_CAP_OUT), 0);
		gpio_pin_set(dev_cap_in, GPIO_PIN(KEYS_CAP_IN), 0);
		k_msleep(10);

		u32_t start_time = k_cycle_get_32();

		uint32_t pin_value = gpio_pin_get(dev_cap_in, GPIO_PIN(KEYS_CAP_IN));

		if (pin_value != 0)
		{
			LOG_WRN("Pin value is still %d, retry", pin_value);
			continue;
		}

		// Start charge
		gpio_pin_set(dev_cap_in, GPIO_PIN(KEYS_CAP_IN), 1);
		gpio_pin_set(dev_cap_out, GPIO_PIN(LEDS_CAP_OUT), 1);

		// wait for while
		u32_t loop;
		for (loop = 0; loop < MAX_LOOP_TO_GO; loop++)
		{
			pin_value = gpio_pin_get(dev_cap_in, GPIO_PIN(KEYS_CAP_IN));

			if (pin_value == 0)
				continue;

			break;
		}
		u32_t end_time = k_cycle_get_32();

		if (loop >= MAX_LOOP_TO_GO)
		{
			LOG_INF("Did not raise!");
		}
		else
		{
			//u32_t dtime = (u32_t)k_cyc_to_ns_floor64(end_time - start_time);
			//LOG_INF("Time: %d / %d", dtime, loop );

			filtered = (90 * filtered + 10 * loop + 50) / 100;
		}


			LOG_INF("Output :%d  (%d)", filtered, loop);

	}
}
