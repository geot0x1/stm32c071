#include "board.h"
#include "delay.h"
#include "fan_control.h"
#include "pwm_repeater.h"
#include "stm32c0xx_hal.h"
#include "temperature_sensor.h"
#include "timers/timers.h"
#include "usb.h"

void temperature_sensor_event_handler(TempSensorEvent event)
{
    switch (event)
    {
    case SensorLost:
        usb_printf("TEMP: SENSOR LOST\r\n");
        break;
    case AboveA:
        usb_printf("TEMP: ABOVE A\r\n");
        break;
    case AboveB:
        usb_printf("TEMP: ABOVE B\r\n");
        break;
    case BelowA:
        usb_printf("TEMP: BELOW A\r\n");
        break;
    case BelowB:
        usb_printf("TEMP: BELOW B\r\n");
        break;
    }
}

int main(void)
{
    HAL_Init();

    board_init();
    timers_init();
    delay_init(timers_get_sys_timer());

    usb_init();

    fan_control_init(timers_get_fan_power(), timers_get_fan_remote());
    fan_init(25000);
    fan_control_set_unit_duty(1, 50);
    fan_control_set_unit_duty(2, 10);
    fan_control_set_unit_duty(3, 60);
    fan_control_set_unit_duty(4, 80);

    temperature_sensor_init();

    pwm_repeater_init(timers_get_capture(), timers_get_repeater_a(),
                      timers_get_repeater_b());

    temperature_sensor_set_setpoint_a(2500);
    temperature_sensor_set_setpoint_b(3000);
    temperature_sensor_set_hysteresis(50);
    temperature_sensor_register_handler(temperature_sensor_event_handler);

    while (1)
    {
        usb_task();
        pwm_repeater_task();
        temperature_sensor_task();

        static uint32_t last_init_debug = 0;
        static uint32_t last_fan_test = 0;
        static bool fan_toggle = false;

        if (HAL_GetTick() - last_fan_test >= 2000)
        {
            last_fan_test = HAL_GetTick();
            fan_toggle = !fan_toggle;

            if (fan_toggle)
            {
                fan_control_set_power_channel_duty(FanChannelTwo, 50);
                fan_control_set_remote_channel_duty(FanChannelOne, 0);
                usb_printf("FAN CH1: POWER ON, REMOTE OFF\r\n");
            }
            else
            {
                fan_control_set_power_channel_duty(FanChannelTwo, 0);
                fan_control_set_remote_channel_duty(FanChannelOne, 50);
                usb_printf("FAN CH1: POWER OFF, REMOTE ON\r\n");
            }
        }

        if (HAL_GetTick() - last_init_debug >= 1000)
        {
            last_init_debug = HAL_GetTick();

            usb_printf("CH_A: %u Hz, %u\r\n", (unsigned int)pwm_get_frequency_a(),
                       (unsigned int)pwm_get_duty_a());
            usb_printf("CH_B: %u Hz, %u\r\n", (unsigned int)pwm_get_frequency_b(),
                       (unsigned int)pwm_get_duty_b());

            uint16_t raw_temp = get_temperature();
            if (raw_temp == 0xFFFF)
            {
                usb_printf("TEMP: SENSOR LOST\r\n");
            }
            else
            {
                usb_printf("TEMP: %u\r\n", raw_temp);
            }
        }
    }
}
