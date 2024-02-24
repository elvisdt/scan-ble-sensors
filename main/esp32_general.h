#ifndef __GENERAL_ESP32_
//--------------------------------------------------
#define __GENERAL_ESP32_

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_sleep.h"


#define BLUE_LED           GPIO_NUM_5
#define LED_READY           BLUE_LED



#define BUF_SIZE 			(512)
#define PACKET_READ_TICS 	(100 / portTICK_PERIOD_MS)

// Timeout threshold for UART = number of symbols (~10 tics) with unchanged state on receive pin
#define ECHO_READ_TOUT      (3)         // 3.5T * 8 = 28 ticks, TOUT=3 -> ~24..33 ticks

#define S_TO_US         1000000
#define MIN_TO_S        60

// Set digital status to HIGH
void activate_pin(int pin_number);

// Set digital status to LOW
void deactivate_pin(int pin_number);

// Configuracion del ESP32
void config_pin_esp32();

// Apagamos los leds
void power_off_leds();

// Obtenemos el tamaño de un string
int get_length(const char* str);

// Leemos el valor de la bateria
float read_battery();
//--------------------------------------------------
#endif /* __GENERAL_ESP32_ */
