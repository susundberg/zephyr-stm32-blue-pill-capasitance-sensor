/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <stdio.h>
#include <drivers/i2c.h>
#include <logging/log.h>
#include <power/reboot.h>

LOG_MODULE_REGISTER(main);

typedef struct
{
    struct sensor_value raw[3];
    float  value[3];
} SensorValue;

static const u32_t AVG_COUNT = 50;
static const float AVG_COUNT_SCALE = 1.0f/AVG_COUNT;

static void convert_values( SensorValue* target )
{
    for ( int loop = 0; loop < 3; loop ++ )
    {
       target->value[loop] = sensor_value_to_double( &(target->raw[loop]) ) * AVG_COUNT_SCALE;
       target->raw[loop].val1 = 0;
       target->raw[loop].val2 = 0;
    }
       
}
static int read_channel( struct device *dev, enum sensor_channel channel, SensorValue* target )
{
    struct sensor_value svalue[3];
    int rc = sensor_channel_get(dev, channel, svalue );
    
    for ( int loop = 0; loop < 3; loop ++ )
    {
	(target->raw[loop]).val1 += svalue[loop].val1;
	(target->raw[loop]).val2 += svalue[loop].val2;
    }
    return rc;
}


static SensorValue ACCEL_VALUE;
static SensorValue GYRO_VALUE;
static SensorValue MANG_VALUE;
static u32_t NSAMPLES = 0;



static int process_mpu6050(struct device *dev)
{
	int rc = sensor_sample_fetch(dev);

    if ( rc != 0 )
        return rc;
    static int nsamples_raw = 0;
    
    read_channel( dev, SENSOR_CHAN_ACCEL_XYZ, &ACCEL_VALUE );
    read_channel( dev, SENSOR_CHAN_GYRO_XYZ, &GYRO_VALUE );
    read_channel( dev, SENSOR_CHAN_MAGN_XYZ, &MANG_VALUE );
    
    nsamples_raw += 1;
    
    if ( nsamples_raw >= AVG_COUNT )
    {
	nsamples_raw = 0;
	convert_values( &ACCEL_VALUE );
	convert_values( &GYRO_VALUE );
	convert_values( &MANG_VALUE );
	
// 	static const float MAG_ADJ[3] = { 0.4265, -0.087,  -0.0755 };
// 	static const float MAG_SCALE[3] = { 0.863,  1.094,  0.419 };
// 	for ( int loop = 0; loop < 3; loop ++ )
// 	{
// 	    MANG_VALUE.value[loop] = (MANG_VALUE.value[loop] - MAG_ADJ[loop]) / MAG_SCALE[loop];
// 	}
	NSAMPLES += 1;
    }
    return rc;
}

#ifdef CONFIG_MPU6050_TRIGGER
static struct sensor_trigger trigger;

static void handle_mpu6050_drdy(struct device *dev,
				struct sensor_trigger *trig)
{
	for ( int retries = 0; retries < 3; retries ++ )
	{
	    
		int rc = process_mpu6050(dev);

		if ( rc == 0 )
		    return;
		
		
		LOG_WRN("Error during transfer (%d), retry!", rc );
	}
		
	LOG_ERR("cancelling trigger due to failure!");
	sensor_trigger_set(dev, trig, NULL);
	return;
}

#endif




#define VEC3_PRINT(x) x.value[0], x.value[1], x.value[2]
void calculate_heading()
{
   printf("%d %0.3f %0.3f %0.3f       %0.3f %0.3f %0.3f      %0.3f %0.3f %0.3f\n", 
          NSAMPLES,
	  VEC3_PRINT( GYRO_VALUE ), 
          VEC3_PRINT( ACCEL_VALUE ), 
          VEC3_PRINT( MANG_VALUE ) );
   NSAMPLES = 0;
}

void main(void)
{
	

#if DT_HAS_NODE(DT_INST(0, invensense_mpu6050))
    const char *const label =  DT_LABEL (DT_INST(0, invensense_mpu6050));
#else
    const char *const label =  DT_LABEL (DT_INST(0, invensense_mpu9250));
#endif
    
	struct device *mpu9250 = device_get_binding(label);
    
	if (!mpu9250) {
		printk("Failed to find sensor %s! Reboot in 100ms.\n", label);
		k_sleep(K_MSEC(100));
		sys_reboot(SYS_REBOOT_COLD);
		
		return;
	}
	
	LOG_INF("Main starting!");



#ifdef CONFIG_MPU6050_TRIGGER
	trigger = (struct sensor_trigger) {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ALL,
	};
	if (sensor_trigger_set(mpu9250, &trigger,
			       handle_mpu6050_drdy) < 0) {
		printf("Cannot configure trigger\n");
		return;
	};
	LOG_INF("Configured for triggered sampling.");
#else
    LOG_INF("Configured for polling sampling");
#endif
   while(1)
   {
       k_sleep(K_MSEC(200));
#ifndef CONFIG_MPU6050_TRIGGER
process_mpu6050( mpu9250 );
#endif
       calculate_heading();
   }

}
