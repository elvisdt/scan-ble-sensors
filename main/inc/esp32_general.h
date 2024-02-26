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


#define BLUE_LED            GPIO_NUM_5
#define LED_READY           BLUE_LED
#define GPIO_ALARMA         GPIO_NUM_13


#define BUF_SIZE 			(512)
#define PACKET_READ_TICS 	(100 / portTICK_PERIOD_MS)

// Timeout threshold for UART = number of symbols (~10 tics) with unchanged state on receive pin
#define ECHO_READ_TOUT      (3)         // 3.5T * 8 = 28 ticks, TOUT=3 -> ~24..33 ticks

#define S_TO_US(x)         (x)*(1e6)
#define MIN_TO_S(x)        (x)*(60)


typedef enum{
    PUB_STATE,
    SUB_STATE,
}state_mqtt_t;


#define SMS_MAX_PARTS 10
/********************************************
 * STRUCUTRES
**********************************************/
typedef struct {
    char** parts;
    int count;
} split_result_t;



// Set digital status to HIGH
void activate_pin(int pin_number);

// Set digital status to LOW
void deactivate_pin(int pin_number);

// Configuracion del ESP32
void config_pin_esp32();

// Apagamos los leds
void power_off_leds();

void remove_newlines(char* str);

char* get_middle_part(char* str);

split_result_t split_msg(char* msg);
//--------------------------------------------------
#endif /* __GENERAL_ESP32_ */
