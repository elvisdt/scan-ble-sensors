#include "esp32_general.h"

void activate_pin(int pin_number){
	gpio_set_level(pin_number, 1);	
	vTaskDelay(10 / portTICK_PERIOD_MS);	
}

void deactivate_pin(int pin_number){
	gpio_set_level(pin_number, 0);	
	vTaskDelay(10 / portTICK_PERIOD_MS);	
}

void activate_alarma(){
	gpio_set_level(GPIO_ALARMA, 0);	
	vTaskDelay(10 / portTICK_PERIOD_MS);	
}

void deactivate_alarma(){
	gpio_set_level(GPIO_ALARMA, 1);	
	vTaskDelay(10 / portTICK_PERIOD_MS);	
}

void config_pin_esp32(){
    gpio_reset_pin( LED_READY );
    gpio_reset_pin(GPIO_ALARMA);

    gpio_pullup_dis(LED_READY );
    gpio_set_direction(LED_READY , GPIO_MODE_OUTPUT );
    gpio_set_direction(GPIO_ALARMA, GPIO_MODE_OUTPUT);
    // Power init pins
    power_off_leds();
    deactivate_alarma();
}

void power_off_leds(){
    deactivate_pin(LED_READY);
}

