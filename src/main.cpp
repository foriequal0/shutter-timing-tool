#include <cstdio>
#include <cinttypes>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"

uint16_t read_adc(uint16_t input) {
    adc_select_input(input);
    return adc_read();
}

bool precondition_strong(bool condition) {
    return condition;
}

bool precondition_weak(bool condition) {
    return condition;
}

int main() {
    {
        gpio_set_function(PICO_DEFAULT_LED_PIN, GPIO_FUNC_PWM);
        auto slice_num = pwm_gpio_to_slice_num(PICO_DEFAULT_LED_PIN);

        auto config = pwm_get_default_config();
        pwm_config_set_clkdiv(&config, 4.f);
        pwm_init(slice_num, &config, true);
    }
    {
        adc_init();
        adc_gpio_init(26); // ADC 0
        adc_gpio_init(27); // ADC 1
        adc_gpio_init(28); // ADC 2
    }
    {
        const auto UART_ID = uart0;
        const auto UART_TX_PIN = 0;
        const auto UART_RX_PIN = 1;
        uart_init(UART_ID, 115200);
        gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
        gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
    }

    const auto threshold = 200;
    while (true) {
        pwm_set_gpio_level(PICO_DEFAULT_LED_PIN, 0);

        absolute_time_t adc0_on = 0;
        absolute_time_t adc0_off = 0;
        absolute_time_t adc1_on = 0;
        absolute_time_t adc1_off = 0;
        absolute_time_t adc2_on = 0;
        absolute_time_t adc2_off = 0;

        // ADC0 on -> ADC1 on -> ADC2 on
        //      \           \           \
        //       ADC0 off -> ADC1 off -> ADC2 off
        while(true) {
            auto now = get_absolute_time();
            // double-check the wiring.
            // adc0 should be the first sensor that is opened/closed (rightmost one from the back of the camera)
            // adc1 should be the middle sensor.
            // adc2 should be the last sensor that is opened/closed (leftmost one from the back of the camera)
            auto adc0 = read_adc(0);
            auto adc1 = read_adc(1);
            auto adc2 = read_adc(2);

            if (adc0_on == 0 && adc0 >= threshold) {
                adc0_on = now;
                pwm_set_gpio_level(PICO_DEFAULT_LED_PIN, 65535);
            }

            if (precondition_weak(adc0_on != 0) &&
                adc1_on == 0 && adc1 >= threshold) {
                adc1_on = now;
            }

            if (precondition_weak(adc1_on != 0) &&
                adc2_on == 0 && adc2 >= threshold) {
                adc2_on = now;
            }

            if (precondition_strong(adc0_on != 0) &&
                adc0_off == 0 && adc0 < threshold) {
                adc0_off = now;
            }

            if (precondition_weak(adc0_off != 0) &&
                precondition_strong(adc1_on != 0) &&
                adc1_off == 0 && adc1 < threshold) {
                adc1_off = now;
            }

            if (precondition_weak(adc1_off != 0) &&
                precondition_strong(adc2_on != 0) &&
                adc2_off == 0 && adc2 < threshold) {
                adc2_off = now;
                break;
            }
        }

        char buf[128];
        size_t offset = 0;
        if (adc1_on != 0 && adc1_off != 0) {
            auto exposure_us = absolute_time_diff_us(adc1_on, adc1_off);
            auto ms = (long)(exposure_us / 1000);
            auto decimal = (int)((exposure_us / 100) % 10);
            offset += sprintf(buf + offset, "exposure: % 4li.%1d ms, ", ms, decimal);
        }

        if (adc0_on != 0 && adc2_on != 0) {
            auto opening_shutter_traversal_us = absolute_time_diff_us(adc0_on, adc2_on);
            offset += sprintf(buf + offset, "opening: % 9li us, ", (long)opening_shutter_traversal_us);
        }

        if (adc0_off != 0 && adc2_off != 0) {
            auto closing_shutter_traversal_us = absolute_time_diff_us(adc0_off, adc2_off);
            offset += sprintf(buf + offset, "closing: % 9li us", (long)closing_shutter_traversal_us);
        }

        sprintf(buf + offset, "\n");
        uart_puts(uart0, buf);
        sleep_ms(100);
    }
}
