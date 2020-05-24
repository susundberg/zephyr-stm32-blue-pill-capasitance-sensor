/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <stdio.h>
#include <logging/log.h>
#include <drivers/adc.h>

LOG_MODULE_REGISTER(main);



#define ADC_DEVICE_NAME         DT_LABEL(DT_INST(0, st_stm32_adc))
#define ADC_RESOLUTION		12
#define ADC_GAIN		ADC_GAIN_1
#define ADC_REFERENCE		ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME	ADC_ACQ_TIME_DEFAULT
#define ADC_1ST_CHANNEL_ID	3
#define ADC_2ST_CHANNEL_ID	8

#define BUFFER_SIZE 8

static s16_t m_sample_buffer[BUFFER_SIZE];


//PA3 == ADC1

static const struct adc_channel_cfg m_1st_channel_cfg = {
	.gain             = ADC_GAIN,
	.reference        = ADC_REFERENCE,
	.acquisition_time = ADC_ACQUISITION_TIME,
	.channel_id       = ADC_1ST_CHANNEL_ID,
};

static const struct adc_channel_cfg m_2st_channel_cfg = {
	.gain             = ADC_GAIN,
	.reference        = ADC_REFERENCE,
	.acquisition_time = ADC_ACQUISITION_TIME,
	.channel_id       = ADC_2ST_CHANNEL_ID,
};


static struct device *init_adc(void)
{
	int ret;
	

	LOG_INF("Init adc: %s", ADC_DEVICE_NAME );
	
	struct device *adc_dev = device_get_binding(ADC_DEVICE_NAME);

	__ASSERT(adc_dev != NULL, "Failed to get device binding");

	ret = adc_channel_setup(adc_dev, &m_1st_channel_cfg);
	__ASSERT(ret == 0, "Failed to config channel %d", ret );
	

	ret = adc_channel_setup(adc_dev, &m_2st_channel_cfg);
	__ASSERT(ret == 0, "Failed to config channel %d", ret );

	
	memset(m_sample_buffer, 0, sizeof(m_sample_buffer));

	return adc_dev;
}


/*
 * test_adc_sample_one_channel
 */


void main(void)
{
	
	LOG_INF("Main starting!");


	struct device *adc_dev = init_adc();

	if (!adc_dev) {
		return;
	}

	while (true) 
	{
	    s32_t values[2];
		test_task_one_channel(adc_dev, ADC_1ST_CHANNEL_ID, &values[0] );
		test_task_one_channel(adc_dev, ADC_2ST_CHANNEL_ID, &values[1] );
		k_sleep(K_MSEC(100));
		LOG_INF("%d %d", values[0], values[1] );
	}
}


