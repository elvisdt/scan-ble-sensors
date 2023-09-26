#ifndef INC_RS485_

#define INC_RS485_
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <esp_task_wdt.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "esp_ota_ops.h"
#include "esp32_general.h"

// Definimos las funciones en la libreria
// y nombres para pines
#define STATUS_Pin          GPIO_NUM_18
#define PWRKEY_Pin          GPIO_NUM_4

#define UART_MODEM          UART_NUM_2
#define BAUD_RATE_M95       115200

// Puerto Serial N2 -> ESP / M95
#define M95_TXD   (17)          // TX de ESP32
#define M95_RXD   (16)          // RX de ESP32
#define M95_RTS   (GPIO_NUM_14)
#define M95_CTS   (GPIO_NUM_27)

#define CLIENT_ID_MQTT  "BLE_SEN"

extern uint8_t rx_modem_ready;
extern bool ota_debug;
extern int rxBytesModem;
extern uint8_t * p_RxModem;

//
void m95_config();

// Funcion con formato para envio de comandos AT
int sendAT(char *Mensaje, char *ok, char *error, uint32_t timeout, char *respuesta);

int M95_check_uart(char* ok_msg, int time);

// Encendemos el M95
void M95_pwron(); 

// Configuramos los estados iniciales del M95
uint8_t M95_begin();
int connect_MQTT_server(int id);

// Verificamos la conexion con el ESP y M95
int M95_CheckConnection(int id);
void M95_checkpower();
// Publicamos la data deseada en el broker
char* get_m95_date();
time_t get_m95_date_epoch();

bool M95_PubMqtt_data(uint8_t * data,char * topic,uint16_t data_len,uint8_t tcpconnectID);
char* get_M95_IMEI();
// Actualizamos la hora interna del ESP
void M95_epoch(time_t * epoch);
// Reseteamos el M95
void M95_reset();
// Vigilamos que no ocurra overflow en el M95
void M95_watchdog();
// Podemos hacer blink en el pin PWRKEY del ESP
void M95_poweroff();
int M95_poweroff_command();
void disconnect_mqtt();
int get_M95_signal();
char M95_readSMS(char*);
void M95_sendSMS(char *mensaje, char *numero);

int readAT(char *ok, char *error, uint32_t timeout, char *response);
uint8_t TCP_open();
uint8_t TCP_send(char *msg, uint8_t len);
void TCP_close();
uint8_t OTA(uint8_t *buff, uint8_t *inicio, uint8_t *fin, uint32_t len);
void reiniciar();
int m95_sub_topic_json(int ID, char* topic_name, char* response);
int m95_sub_topic_OTA(int ID, char* topic_OTA, char* response);
void m95_unsub_topic(int ID, char* topic_name);


#endif /* INC_CS_RS485 */
