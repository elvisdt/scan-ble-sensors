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

float get_json_value(const char *json_string, const char *char_find, float def, int *sucess){
  int len_find = get_length(char_find);
  char* p_find;
  p_find = char_find;

  int matched;
  matched = 0;

  while(*json_string != '\0'){
    if(*json_string == *p_find){
      while(*json_string == *p_find){
        if(*json_string == '\0') break;
        if(*p_find == '\0') break;
        matched++;
        json_string++;
        p_find++;
      }
    
      if(matched == len_find){
        break;
      }
      matched = 0;
      p_find = char_find;
    }
    json_string++;
  }

  if(matched != len_find){
    *sucess = 0;
    return def;
  }

  matched = 0;
  int len_float = 0;

  while(matched <3){
    if(*json_string == '\"'){
      matched++;
    }
    json_string++;
    if(*json_string == '\"'){
      continue;
    }
    if(matched == 2){
      len_float++;
    }
  }

  json_string--;
  char value[len_float];

  for(int m = 0; m<len_float; m++){
    json_string--;
    value[len_float-m-1] = *json_string;
  }

  *sucess = 1;
  //printf("Resultado = %s se detecto el valor\n",(*sucess == 1? "SI":"NO"));
  return atof(value);
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

