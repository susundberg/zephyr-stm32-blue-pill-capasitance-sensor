

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
#define RET_CHECK(x) { int32_t __ret_check = (x); if ( __ret_check != 0 ) { LOG_ERR("Call failed: %d", __ret_check ); __ASSERT_NO_MSG(0);}};




LOG_MODULE_REGISTER(main);

#define LOCAL_queue_n 4
static struct k_msgq LOCAL_queue ;
static u32_t LOCAL_queue_buffer[ LOCAL_queue_n ];

static struct gpio_callback LOCAL_ISR_signal;

static void signal_isr(struct device* gpiob, struct gpio_callback* cb, u32_t pins)
{
    (void)gpiob;
    (void)pins;
    (void)cb;
	u32_t end_time = k_cycle_get_32();
	k_msgq_put( &LOCAL_queue, &end_time, K_NO_WAIT );
}

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
		// __ASSERT_NO_MSG(0);
		return;
	}
}

#define GPIO_PIN(name) (DT_GPIO_##name##_GPIOS_PIN)
#define GPIO_CONFIGURE(dev, name, flags)                                                          \
	{                                                                                             \
		get_and_check(dev, DT_GPIO_##name##_GPIOS_CONTROLLER, DT_GPIO_##name##_GPIOS_PIN, flags); \
	}

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

    k_msgq_init( &LOCAL_queue, (char*)LOCAL_queue_buffer, 4, LOCAL_queue_n );

	gpio_init_callback( &LOCAL_ISR_signal, signal_isr, BIT( GPIO_PIN(KEYS_CAP_IN) ) );
    RET_CHECK( gpio_add_callback( dev_cap_in, &LOCAL_ISR_signal) );
    RET_CHECK( gpio_pin_interrupt_configure( dev_cap_in, GPIO_PIN(KEYS_CAP_IN), GPIO_INT_EDGE_RISING) );


	//	GPIO_CONFIGURE(&dev_cap_in, KEYS_CAP_IN ,  GPIO_ACTIVE_HIGH| GPIO_OUTPUT  );

	u32_t filtered = 0;
	u32_t dtime = 0;
	u32_t mainloop = 0;
	for (;;)
	{
		mainloop += 1;
		gpio_pin_set(dev_led, GPIO_PIN(LEDS_LED0), (int)led_is_on);
		led_is_on = !led_is_on;
		// k_msleep(SLEEP_TIME_MS);

		// Empty charge
		gpio_pin_set(dev_cap_out, GPIO_PIN(LEDS_CAP_OUT), 0);
		gpio_pin_set(dev_cap_in, GPIO_PIN(KEYS_CAP_IN), 0);
		k_msgq_purge( &LOCAL_queue );
		k_msleep(10);

		

		uint32_t pin_value = gpio_pin_get(dev_cap_in, GPIO_PIN(KEYS_CAP_IN));

		if (pin_value != 0)
		{
			LOG_WRN("Pin value is still %d, retry", pin_value);
			continue;
		}

		// Start charge
		gpio_pin_set(dev_cap_in, GPIO_PIN(KEYS_CAP_IN), 1);
		u32_t start_time = k_cycle_get_32();
		
		gpio_pin_set(dev_cap_out, GPIO_PIN(LEDS_CAP_OUT), 1);
		
		u32_t end_time = 0;
		if ( k_msgq_get( &LOCAL_queue, &end_time, K_MSEC( SLEEP_TIME_MS ) ) == 0  )
		{
			// Got new message
			if ( end_time > start_time )
			{
			   dtime = (u32_t) ( k_cyc_to_ns_floor64(end_time - start_time) );
			   filtered = (950 * filtered + 50 * dtime + 500) / 1000;
			   
			}
			else
			{
				dtime = 0;
				LOG_INF("Overflow!");
			}
		}
		else
		{
			dtime = 1;
			LOG_INF("Did not observe signal!");
		}
		
		
		if ( ( mainloop > 10 ) != 0 )
		{
			const char* stype = "    ";
			if ( filtered >= 80000 )
			{
				stype = ("SOIL");
			}

			LOG_INF("%s        Time: %d / %d", stype, filtered, dtime );
			mainloop = 0;
		}


	}
}
