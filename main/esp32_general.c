#include "esp32_general.h"

void activate_pin(int pin_number){
	gpio_set_level(pin_number, 1);	
	vTaskDelay(10 / portTICK_PERIOD_MS);	
}

void deactivate_pin(int pin_number){
	gpio_set_level(pin_number, 0);	
	vTaskDelay(10 / portTICK_PERIOD_MS);	
}

int get_length(const char* str){
  int l = 0;
  while(*str != '\0'){
    l++;
    str++;
  }
  return l;
}


void config_pin_esp32(){

    gpio_reset_pin( LED_READY );
    gpio_pullup_dis(LED_READY );

    gpio_set_direction( LED_READY , GPIO_MODE_OUTPUT );


    // Power init pins
    power_off_leds();
}

void power_off_leds(){
    deactivate_pin(LED_READY );
}

