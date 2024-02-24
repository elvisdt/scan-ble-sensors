#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_system.h"
#include "esp_timer.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"


#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <sys/time.h>


#include "inkbird_ble.h"
#include "esp32_general.h"
#include "M95_uart.h"
#include "ota_m95.h"

#include "crc.h"
#include "ota_control.h"
#include "ota_headers.h"
#include "ota_esp32.h"
#include "ota_global.h"

#include <cjson.h>
#include "sdkconfig.h"

/*--------------------------
        DEFINITIONS          
---------------------------*/

// BLE ID
#define PROFILE_A_APP_ID        0

// Read packet timeout
#define TAG                             "BLE"        // Nombre del TAG

#define MODEM_TASK_STACK_SIZE           (2048)
#define BUF_SIZE_MODEM                  (1024)
#define RD_BUF_SIZE                     (BUF_SIZE_MODEM)


#define MASTER_TOPIC "IVAN_BLE/" 

// RTC_DATA_ATTR int gpio_alarm;
RTC_DATA_ATTR int status_led = 0;
RTC_DATA_ATTR int status_modem = 0;

/*--------------------------
        STRUCTURES           
---------------------------*/

struct datos_modem{
    char    IMEI[25];
    char    topic[50];
};

struct datos_modem m95 = {
                    .IMEI = "0",
                    .topic = MASTER_TOPIC,
                };




/*--------------------------
        VARIABLES           
---------------------------*/
//---BLE--//


cJSON *doc;                     // Formato JSON para enviar confirmacion de OTA
                                // al servidor OTA
char * output;                  // Valor del formato JSON

static QueueHandle_t uart_modem_queue; 
//-------------
TaskHandle_t MQTT_send_handle = NULL;
TaskHandle_t MODEM_event_handle = NULL;
TaskHandle_t M95_Task_handle = NULL;
nvs_handle_t storage_nvs_handle;
esp_err_t err;

int ota_value=0;                 // Valor para saber si el usuario quiere programar por OTA
                                // mediante suscripcion al topico /OTA

int end_task_uart_m95; 

struct timeval tv;
struct tm* timeinfo;
uint32_t current_time=0;
uint32_t OTA_time=0;

time_t unix_time=0;
int tcpconnectID=0;


uint8_t rx_modem_ready;
int rxBytesModem;
uint8_t * p_RxModem;
bool ota_debug = false; 



/*--------------------------
            FUNCTIONS
---------------------------*/
void OTA_check(void);               // Funcion para verificar que el dispositivo requiere OTA



/*----------------------
        CODE BLE        
----------------------*/
#define SCAN_DURATION 30            // Duración del escaneo en segundos
#define TARGET_DEVICE_NAME "sps"    // Nombre del dispositivo BLE 

static bool scanning = false;
static size_t sen_ble_count = 0;
sensor_ble sensor_ble_data[MAX_BLE_DEVICES];


static esp_ble_scan_params_t ble_scan_params = {
    // Inicializa aquí los parámetros de escaneo BLE
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x200,
    .scan_window            = 0x100,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_ENABLE
    
};

//--------------------------------------------//
void ble_scanner_init(void);
void ble_scanner_start(void);
void ble_scanner_stop(void);
void Format_data(char *mensaje_sensor, sensor_ble* sensor_data);
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param);


void ble_scanner_init(void) {
    // Initialize NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret =  esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }


    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    //register the  callback function to the gap module
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret){
        ESP_LOGE(TAG, "%s gap register failed, error code = %x", __func__, ret);
        return;
    }

    ret = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
    if (ret){
        ESP_LOGE(TAG, "%s gattc app register failed, error code = %x", __func__, ret);
    }
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret){
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

    
}

void ble_scanner_start(void) {
    if (!scanning) {
        esp_ble_gap_set_scan_params(&ble_scan_params); 
        esp_ble_gap_start_scanning(SCAN_DURATION);
        scanning = true;
        printf("Started BLE scanning...\n\r");
    }
}

void ble_scanner_stop(void) {
    if (scanning) {
        esp_ble_gap_stop_scanning();
        scanning = false;
        ESP_LOGI(TAG, "Stopped BLE scanning");
    }
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {

    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;

    uint8_t *manufacturer_data;
    uint8_t adv_manufacturer_len = 0;

    switch (event) {
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:{
            // deshabilitar si se completa los dispositivos máximos
            if (sen_ble_count >= MAX_BLE_DEVICES) {
                scanning =false;
                break;
            }

            esp_log_buffer_hex(TAG, scan_result->scan_rst.bda, 6);
            adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL,&adv_name_len);

            if (adv_name != NULL) {
                if (strlen(TARGET_DEVICE_NAME) == adv_name_len && strncmp((char *)adv_name, TARGET_DEVICE_NAME, adv_name_len) == 0) {
                    
                    manufacturer_data = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                            ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE,
                                            &adv_manufacturer_len);
                   
                    int index_adr = Inkbird_mac_index(sensor_ble_data,scan_result->scan_rst.bda);

                    if (index_adr==-1)
                    {
                        //printf("searched device: %s \n\R", TARGET_DEVICE_NAME);
                        ESP_LOGI(TAG,"searched device: %s \n", TARGET_DEVICE_NAME);
                        strncpy(sensor_ble_data[sen_ble_count].Name, (char *)adv_name, sizeof(sensor_ble_data[sen_ble_count].Name));
                        sensor_ble_data[sen_ble_count].Name[adv_name_len] = '\0';  // final de la cadena
                        memcpy(sensor_ble_data[sen_ble_count].addr, scan_result->scan_rst.bda, 6);
                        memcpy(sensor_ble_data[sen_ble_count].data_Hx, manufacturer_data, adv_manufacturer_len);
                        
                        gettimeofday(&tv, NULL);
						unix_time = tv.tv_sec;
                        sensor_ble_data[sen_ble_count].epoch_ts = unix_time;
                        sen_ble_count++;

                    } 
                    else if(index_adr!=-1 && adv_manufacturer_len==9){
                        ESP_LOGI(TAG,"searched device: %s \n", TARGET_DEVICE_NAME);
                        strncpy(sensor_ble_data[index_adr].Name, (char *)adv_name, sizeof(sensor_ble_data[index_adr].Name));
                        sensor_ble_data[index_adr].Name[adv_name_len] = '\0';  // final de la cadena
                        memcpy(sensor_ble_data[index_adr].addr, scan_result->scan_rst.bda, 6);
                        memcpy(sensor_ble_data[index_adr].data_Hx, manufacturer_data, adv_manufacturer_len);
                        
                        gettimeofday(&tv, NULL);
						unix_time = tv.tv_sec;
                        sensor_ble_data[index_adr].epoch_ts = unix_time;
                    }
                }
            }
            break;
        }
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            ESP_LOGI("BLE", "End scan...\n");
            scanning = false;
            break;
        default:
            break;
        }
        break;
        }
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: {
        scanning = false;
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Scan stop failed, error status = %x", param->scan_stop_cmpl.status);
        }
        break;
        }
    default:
        break;
    }
}





/*-----------------
        TASKS      
------------------*/

// Funcion para eventos en el puerto serial
static void M95_rx_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);
    p_RxModem = dtmp;
    int ring_buff_len;
    for(;;) {
        //Waiting for UART event.
        if(xQueueReceive(uart_modem_queue, (void * )&event, (TickType_t )portMAX_DELAY)) {
            if(end_task_uart_m95 > 0){
                break;
            }
            switch(event.type) {
                case UART_DATA:
                    if(event.size >= 120){
                        ESP_LOGI(TAG, "OVER [UART DATA]: %d", event.size);
                        vTaskDelay(200/portTICK_PERIOD_MS);
                    }
                    if(rx_modem_ready == 0){
                        bzero(dtmp, RD_BUF_SIZE);
                        uart_get_buffered_data_len(UART_MODEM,(size_t *)&ring_buff_len);
                        rxBytesModem = uart_read_bytes(UART_MODEM,dtmp,ring_buff_len,0);                        
                        rx_modem_ready = 1;
                    }
                    break;
                    
                default:
                    break;
            }
        }
        vTaskDelay(100/portTICK_PERIOD_MS);
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}


// Tarea principal donde leemos y enviamos los datos a la database//

static void _main_task(){

    //char* msg_mqtt   = (char*) malloc(256); 
    char* mensaje_recv = (char*) malloc(256); 
    int ret = 0;

    // Obtenemos el IMEI
    strcpy(m95.IMEI,get_M95_IMEI());
    // Seteamos el topico donde se publicara los resultados
    strcat(m95.topic,m95.IMEI);
    strcat(m95.topic,"/DATA");

    int64_t ble_timer = esp_timer_get_time() - 170000000;
    int64_t OTA_timer = esp_timer_get_time() ;
    ble_scanner_init();

    while (true)
    {   
        
        current_time=pdTICKS_TO_MS(xTaskGetTickCount())/1000;

        if (current_time%30==0){
            ESP_LOGI("TIME ", "%ld s",current_time);
            //ESP_LOGE("OTA ", "Version 1.1");
		}

        if(esp_timer_get_time() - ble_timer > 180000000){
            ble_scanner_start();
			vTaskDelay(1000/portTICK_PERIOD_MS);
            do{
                vTaskDelay(5000/portTICK_PERIOD_MS);
            }while (scanning);
            
            char DataBase[500] = "";
            for (size_t i = 0; i < sen_ble_count; i++)
            {
                if (i==0){
                    strcat(DataBase, "{\r\n\"sensores\":[\r\n");
                }
                Format_data(mensaje_recv, &sensor_ble_data[i]);
                strcat(DataBase, mensaje_recv);
                if (i < sen_ble_count - 1) {
                    strcat(DataBase, ",\r\n");
                }

                if(i==sen_ble_count - 1){
                    strcat(DataBase, "\r\n]}\r\n");
                }
            }
            
            if (sen_ble_count>0)
            {
                printf("%s\n", DataBase);

                // Establecer conexion MQTT
                ret = connect_MQTT_server(tcpconnectID);
                
                // Publicamos el valor leido
                int intentos_mqtt = 0;
                while (true && ret==1)
                {
                    intentos_mqtt ++;
                    if( M95_PubMqtt_data((uint8_t*)DataBase,m95.topic,strlen(DataBase),0) ){
                        vTaskDelay(500/portTICK_PERIOD_MS);
                        ESP_LOGI("BLE", "PUBLICANDO ...");
                        char* fecha_n = get_m95_date();
                        if (fecha_n != NULL) {
                            ESP_LOGI(TAG,"Fecha: %s",fecha_n);
                        } 

                        break;
                    }
                    
                    // mas de 5 intentos reset
                    if(intentos_mqtt>5){
                        vTaskDelay(1000/portTICK_PERIOD_MS);
                        esp_restart();
                        break;
                    }
                    // volver a intentar conectarse
                    vTaskDelay( 500 / portTICK_PERIOD_MS );
                    ret= M95_CheckConnection(tcpconnectID);
                }

                // Desconectar MQTT
                disconnect_mqtt();

            }
            
            memset(DataBase, 0, sizeof(DataBase));

            ble_timer = esp_timer_get_time();			
		}

        if (esp_timer_get_time() - OTA_timer> 60000000){
            current_time=pdTICKS_TO_MS(xTaskGetTickCount())/1000;
            OTA_timer = esp_timer_get_time();
            OTA_check();
			printf("Siguiente ciclo en 60 segundos\r\n");
			printf("OTA CHECK tomo %ld segundos\r\n",(pdTICKS_TO_MS(xTaskGetTickCount())/1000-current_time));
			vTaskDelay(1000 / portTICK_PERIOD_MS);

        }


        vTaskDelay(5000/ portTICK_PERIOD_MS );
    }
    
    //power_off_leds();
    free(mensaje_recv);

    vTaskDelete(NULL);
}



void OTA_check(void){
  char buffer[700];

  static const char *OTA_TAG = "OTA_task";
  esp_log_level_set(OTA_TAG, ESP_LOG_INFO);

  printf("OTA:Revisando conexion...\r\n");
  //bool repuesta_ota = false;
  //int intentos = 0;
  do{
      //intentos++;
      //printf("Intento n°%d\r\n",intentos);
      if(!TCP_open()){
		ESP_LOGI(OTA_TAG,"No se conecto al servidor\r\n");
		TCP_close();
		printf("OTA:Desconectado\r\n");
		break;
	  }
         
	  ESP_LOGI(OTA_TAG,"Solicitando actualizacion...\r\n");
	  if(TCP_send(output, strlen(output))){            // 1. Se envia la info del dispositivo
		
        printf("\t OTA : Esperando respuesta...\r\n");
		readAT("}\r\n", "-8/()/(\r\n",10000,buffer);   // 2. Se recibe la 1ra respuesta con ota True si tiene un ota pendiente... (el servidor lo envia justo despues de recibir la info)(}\r\n para saber cuando llego la respuesta)
		
        debug_ota("main> repta %s\r\n", buffer);

		if(strstr(buffer,"\"ota\": \"true\"") != 0x00  ){
            //repuesta_ota = true;
            ESP_LOGI(OTA_TAG,"Iniciando OTA");

			if(ota_uartControl_M95() == OTA_EX_OK){
			    debug_ota("main> OTA m95 Correcto...\r\n");
                esp_restart();    
			}
			else{
			  debug_ota("main> OTA m95 Error...\r\n");
			}
			printf("Watchdog reactivado\r\n");
		}
		ESP_LOGE("OTA ERROR","No hubo respuesta\r\n");
	  }

      //vTaskDelay(2000);
	}
    while(false);
	//while(!repuesta_ota&(intentos<3));
}

void Format_data(char *mensaje_sensor, sensor_ble* sensor_data){
    char buffer_MAC[20];

    float temperatura =  Inkbird_temperature(sensor_data->data_Hx);
    float humedad = Inkbird_humidity(sensor_data->data_Hx);
    float bateria = Inkbird_battery(sensor_data->data_Hx);
    char *MAC = Inkbird_MAC_to_str(sensor_data, buffer_MAC);

    char hex_str[3];
    char data_Hx_str[sizeof(sensor_data->data_Hx)*2 + 1] = "";
    for (int i = 0; i < sizeof(sensor_data->data_Hx); i++) {
        sprintf(hex_str, "%02x", sensor_data->data_Hx[i]);
        strcat(data_Hx_str, hex_str);
    }

    sprintf(mensaje_sensor,
    "{\r\n"
    "\"MAC\":\"%s\",\r\n"
    "\"Temp\":%.2f,\r\n"
    "\"Hum\":%.2f,\r\n"
    "\"Bat\":%.2f,\r\n"
    "\"Fecha\":%lld,\r\n"
    "\"Hex\":\"%s\"\r\n"
    "}",
    MAC,
    temperatura,
    humedad,
    bateria,
    sensor_data->epoch_ts,
    data_Hx_str
    );
}

void m95_config(){
	// GPIO PIN CONFIGURATION
	gpio_reset_pin(STATUS_Pin);
	gpio_reset_pin(PWRKEY_Pin);
    ESP_ERROR_CHECK(gpio_pulldown_en(STATUS_Pin));
    
    gpio_set_direction(STATUS_Pin, GPIO_MODE_INPUT);
    gpio_set_direction(PWRKEY_Pin, GPIO_MODE_OUTPUT);

	// SERIAL PORT CONFIGURATION
	const int uart_m95 = UART_MODEM;
    uart_config_t uart_config_modem = {
		.baud_rate = BAUD_RATE_M95,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_APB,
	};

    // Install UART driver (we don't need an event queue here)
    ESP_ERROR_CHECK(uart_driver_install(UART_MODEM, BUF_SIZE_MODEM * 2, BUF_SIZE_MODEM * 2, 20, &uart_modem_queue, 0));
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_MODEM, &uart_config_modem));
    // Set UART pins as per KConfig settings
    ESP_ERROR_CHECK(uart_set_pin(uart_m95, M95_TXD, M95_RXD, M95_RTS, M95_CTS));
    // Set read timeout of UART TOUT feature
    //ESP_ERROR_CHECK(uart_set_rx_timeout(uart_m95, ECHO_READ_TOUT));

	// Modem start: OFF
	deactivate_pin(PWRKEY_Pin);
}


void app_main(void)
{   
    printf("---- Iniciando programa ... \n");
    end_task_uart_m95 = 0;

    // ---------------------------------------------------------------
    ESP_LOGI("Init","Begin Configuration:\nPins Mode Input, Output\n");
    config_pin_esp32();
    m95_config();

    //---------------------------------------------------------------
    xTaskCreate(M95_rx_event_task, "M95_rx_event_task", 4096, NULL, 12, NULL);
    M95_checkpower();

    ESP_LOGI("M95","Iniciando modulo ...");
    int intentos_m95=0;   
    while (1){
        intentos_m95++;
        if(M95_begin() == 1){
            break;
        }
        else if(intentos_m95 >5){
            M95_poweroff();
            vTaskDelay(1000/portTICK_PERIOD_MS);
            esp_restart();
        }

        ESP_LOGI("M95","Intento Inicialización N°: %i",intentos_m95);	
        vTaskDelay(2000/portTICK_PERIOD_MS);
    }
    
    // ---------------------------------------------------------------
    strcpy(m95.IMEI,get_M95_IMEI());
    char* fecha_n = get_m95_date();
    time_t fecha_epoch = get_m95_date_epoch();

    ESP_LOGI(TAG,"IMEI: %s", m95.IMEI );
    if (fecha_n != NULL) {
        ESP_LOGI(TAG,"Fecha: %s",fecha_n);
    } 
    
    if(fecha_epoch != -1) {
        ESP_LOGI(TAG,"Fecha epoch: %lld \n",fecha_epoch);
    } else {
        ESP_LOGE(TAG,"Hubo un error al obtener la fecha y hora \n");
    }
    
    ESP_LOGI(TAG,"Iniciando Configuracion Sensores BLE ");
    // ble_scanner_init();

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG,"Configuración finalizada");
    ESP_LOGI(TAG,"Iniciando tarea principal");

    doc = cJSON_CreateObject();
    cJSON_AddItemToObject(doc,"imei",cJSON_CreateString(m95.IMEI));
    cJSON_AddItemToObject(doc,"project",cJSON_CreateString("TH"));
    cJSON_AddItemToObject(doc,"ota",cJSON_CreateString("true"));
    cJSON_AddItemToObject(doc,"cmd",cJSON_CreateString("false"));
    cJSON_AddItemToObject(doc,"sw",cJSON_CreateString("1.1"));
    cJSON_AddItemToObject(doc,"hw",cJSON_CreateString("1.1"));
    cJSON_AddItemToObject(doc,"otaV",cJSON_CreateString("1.1"));
    output = cJSON_PrintUnformatted(doc);

    printf("Mensaje OTA:\r\n");
    printf(output);
    printf("\r\n");

    
    xTaskCreate(_main_task, "_main_task", 6144, NULL, 15, NULL);

    gpio_reset_pin(GPIO_NUM_13);
    gpio_set_direction(GPIO_NUM_13, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_13,0);
    vTaskDelay(5000/portTICK_PERIOD_MS);
    gpio_set_level(GPIO_NUM_13,1);
    vTaskDelay(2000/portTICK_PERIOD_MS);

}