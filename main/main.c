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
#include <sys/time.h>

#include "esp_gatt_common_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_log.h"
#include "esp_bt.h"



#include "esp_log.h"


#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "math.h"

#include "inkbird_ble.h"
#include "esp32_general.h"
#include "M95_uart.h"
#include "ota_m95.h"

#include "crc.h"
#include "ota_control.h"
#include "ota_headers.h"
#include "ota_esp32.h"
#include "ota_global.h"

#include "cJSON.h"
#include "sdkconfig.h"

/*****************************************************
 * DEFINITIONS    
*****************************************************/


// BLE ID
#define PROFILE_A_APP_ID        0

// Read packet timeout
#define TAG                             "BLE"        // Nombre del TAG

#define BUFFER_SIZE_DATA                (512) // Define el tamaño del búfer según tus necesidades
#define MODEM_TASK_STACK_SIZE           (2048)
#define BUF_SIZE_MODEM                  (1024)
#define RD_BUF_SIZE                     (BUF_SIZE_MODEM)


#define MASTER_TOPIC "IVAN_BLE" 

// RTC_DATA_ATTR int gpio_alarm;
RTC_DATA_ATTR int status_led = 0;
RTC_DATA_ATTR int status_modem = 0;


// BLE
#define SCAN_DURATION 20            // Duración del escaneo en segundos
#define TARGET_DEVICE_NAME "sps"    // Nombre del dispositivo BLE 

#define INTERVAL_OTA_SEG    120   // 60 SEG
#define INTERVAL_MQTT_SEG   60   // 90 SEG
#define INTERVAL_BLE_SCAN   120   // 30 SEG

/*****************************************************
 * ESTRUCTURAS
*****************************************************/

static esp_ble_scan_params_t ble_scan_params = {
    // Inicializa aquí los parámetros de escaneo BLE
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x200,
    .scan_window            = 0x100,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_ENABLE
    
};


/*****************************************************
 * VARIABLES
*****************************************************/
static char     IMEI[25];
static char     TopicBle[150];

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

int end_task_uart_m95; 

struct timeval tv;
struct tm* timeinfo;
uint32_t current_time=0;
uint32_t OTA_time=0;

time_t unix_time = 0;
int tcpconnectID = 0; // tcp id =0


uint8_t rx_modem_ready;
int rxBytesModem;
uint8_t * p_RxModem;
bool ota_debug = false; 


static bool scanning = false;
static size_t sen_ble_count = 0;
sens_ble_t  sens_ble [MAX_BLE_DEVICES]={0};
static  data_sens_t data_sens_ble[MAX_BLE_DEVICES]={0};
static sub_sens_t  list_sens_sub[MAX_BLE_DEVICES]={0};


/*****************************************************
 *      FUNCTIONS   
*****************************************************/
void OTA_check(void);               // Funcion para verificar que el dispositivo requiere OTA

//--------------------------------------------//
void ble_scanner_init(void);
void ble_scanner_start(void);
void ble_scanner_stop(void);
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param);
int convert_ble_sens_data(sens_ble_t sensor, data_sens_t *data);
void convert_data_sens_to_json(data_sens_t* data, char* buffer, size_t sen_ble_count);
void data_sens_to_json(data_sens_t data, char* buffer);
int mqtt_main_data_pub(char* topic, char * data_str, uint8_t len);

int mqtt_main_data_sub(char *ble_mac, sub_sens_t* data_sub);
int parse_json_to_alert(const char* json_string, sub_sens_t* data);

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

    // Initialize Bluedroid with configuration
    esp_bluedroid_config_t bluedroid_cfg = {
        .ssp_en = true  // Habilita el emparejamiento seguro simple (SSP)
    };

    ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
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
                   
                    int index_adr = Inkbird_mac_index(sens_ble,scan_result->scan_rst.bda);

                    if (index_adr==-1)
                    {
                        //printf("searched device: %s \n\R", TARGET_DEVICE_NAME);
                        ESP_LOGI(TAG,"searched device: %s \n", TARGET_DEVICE_NAME);
                        strncpy(sens_ble[sen_ble_count].Name, (char *)adv_name, sizeof(sens_ble[sen_ble_count].Name));
                        sens_ble[sen_ble_count].Name[adv_name_len] = '\0';  // final de la cadena
                        memcpy(sens_ble[sen_ble_count].addr, scan_result->scan_rst.bda, 6);
                        memcpy(sens_ble[sen_ble_count].data_Hx, manufacturer_data, adv_manufacturer_len);
                        
                        gettimeofday(&tv, NULL);
						unix_time = tv.tv_sec;
                        sens_ble[sen_ble_count].epoch_ts = unix_time;
                        sen_ble_count++;

                    } 
                    else if(index_adr!=-1 && adv_manufacturer_len==9){
                        ESP_LOGI(TAG,"searched device: %s \n", TARGET_DEVICE_NAME);
                        strncpy(sens_ble[index_adr].Name, (char *)adv_name, sizeof(sens_ble[index_adr].Name));
                        sens_ble[index_adr].Name[adv_name_len] = '\0';  // final de la cadena
                        memcpy(sens_ble[index_adr].addr, scan_result->scan_rst.bda, 6);
                        memcpy(sens_ble[index_adr].data_Hx, manufacturer_data, adv_manufacturer_len);
                        
                        gettimeofday(&tv, NULL);
						unix_time = tv.tv_sec;
                        sens_ble[index_adr].epoch_ts = unix_time;
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




void update_scaner_sensors(){
    ble_scanner_start();
    vTaskDelay(pdMS_TO_TICKS(1000));
    do{
        vTaskDelay(pdMS_TO_TICKS(1000));
    }while (scanning);

    //--------------------//
    if (sen_ble_count>0){
        for (size_t i = 0; i < sen_ble_count; i++){
            convert_ble_sens_data(sens_ble[i], &data_sens_ble[i]);
        }
    }
}

int set_id_alert(data_sens_t* sens_ble, sub_sens_t data){
    for (size_t i = 0; i < sen_ble_count; i++){
        if (strcmp(sens_ble[i].mac, data.mac) == 0){
            sens_ble->id= data.id;
            sens_ble->temp_alert = data.ta;
            return 1;
        }
    }
    return 0;
}

// Tarea principal donde leemos y enviamos los datos a la database//

static void main_task(void *pvParameters){

    //char* msg_mqtt   = (char*) malloc(256); 
    int ret = 0;

    // Seteamos el topico donde se publicara los resultados
    // sprintf(TopicBle,"%s/%s/DATA",MASTER_TOPIC,IMEI);

    time_t time_modem;
    time(&time_modem);
    
    time_modem = time_modem - INTERVAL_MQTT_SEG;

    time_t OTA_timer = time_modem;
    time_t ble_timer = time_modem;
    time_t mqtt_timer = time_modem;

    ble_scanner_init();
    state_mqtt_t state_mqtt = SUB_STATE;

    while (true){
        vTaskDelay(pdMS_TO_TICKS(2000)); 
        time(&time_modem);
        if (difftime(time_modem,ble_timer)>INTERVAL_BLE_SCAN){
            update_scaner_sensors();
        }
        

        if(difftime(time_modem,mqtt_timer)>INTERVAL_MQTT_SEG){
            mqtt_timer = time_modem;

            switch (state_mqtt){
                case SUB_STATE:
                    ESP_LOGI(TAG, "---------SUB STATE---------");
                    state_mqtt = PUB_STATE;
                    ret = connect_MQTT_server(tcpconnectID);
                    if(ret ==1){
                        for (size_t i = 0; i < sen_ble_count; i++){
                            char ble_mac[20] ={0};
                            Inkbird_MAC_to_str_1(sens_ble[i], ble_mac); 
                            ret = mqtt_main_data_sub(ble_mac, &list_sens_sub[i]);
                            if(ret!=1){
                                break;
                            }
                        }
                    }
                    disconnect_mqtt(tcpconnectID);
                    // SUSCRIBE
                    break;
                case PUB_STATE:{
                    ESP_LOGI(TAG,"----------PUB STATE---------");

                    // Imprimir datos de los sensores solo si se encontraron en el JSON
                    for (size_t i = 0; i < sen_ble_count; i++){
                        set_id_alert(&data_sens_ble[i],list_sens_sub[i]);
                    }
                    state_mqtt = SUB_STATE;
                    if (sen_ble_count>0){
                        // procesar data de los sensores:

                        // coenctar 
                        // Establecer conexion MQTT
                        ret = connect_MQTT_server(tcpconnectID);
                        // Publicamos el valor leido
                        char buffer_data[BUFFER_SIZE_DATA/2];
                        int count = 0;
                        for (size_t i = 0; i <  sen_ble_count; i++){
                            sprintf(TopicBle,"%s/%s/%s/data",MASTER_TOPIC,IMEI,data_sens_ble[i].mac);
                            data_sens_to_json(data_sens_ble[i],buffer_data);
                            ret = mqtt_main_data_pub(TopicBle, buffer_data, strlen(buffer_data));
                            if (ret!=1){
                                break;
                            }
                            count ++;
                            vTaskDelay(pdMS_TO_TICKS(500));
                        }
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        disconnect_mqtt();
                        printf ("number send data: %d\n",count);
                        printf("buff: %s\n",buffer_data);
                    }
                    }break;
                default:
                    break;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            time(&time_modem);		
		}

        if (difftime(time_modem, OTA_timer)> INTERVAL_OTA_SEG){
            OTA_timer = time_modem;
            OTA_check();
			printf("Siguiente ciclo en %d segundos\r\n", INTERVAL_OTA_SEG);
			vTaskDelay(pdMS_TO_TICKS(1000));
        }

    }
    //power_off_leds();
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
	}while(false);
	//while(!repuesta_ota&(intentos<3));
}

int mqtt_main_data_pub(char* topic, char * data_str, uint8_t len){
    int ret = 1;

    int intentos_mqtt = 0;
    while (true && ret==1){
        intentos_mqtt ++;
        if( M95_PubMqtt_data((uint8_t*)data_str, TopicBle, len, tcpconnectID) ){
            vTaskDelay(500/portTICK_PERIOD_MS);
            ESP_LOGI("BLE", "PUBLICANDO ...");
            printf("data: %s\n",data_str);
            return 1;
        }
        // mas de 5 intentos reset
        if(intentos_mqtt>3){
            return 0;
        }
        // volver a intentar conectarse
        vTaskDelay(pdMS_TO_TICKS(500));
        ret= M95_CheckConnection(tcpconnectID);
    }
    return ret;
}

int mqtt_main_data_sub(char *ble_mac, sub_sens_t* data_sub){
    int ret=0;
    char topic_sub[100]={0};
    char* msg_sub_topic  = (char*) malloc(BUFFER_SIZE_DATA/2);

    sprintf(topic_sub,"%s/%s/%s/alert",MASTER_TOPIC,IMEI, ble_mac);
    // ret = 
    ret = M95_SubTopic(tcpconnectID, topic_sub, msg_sub_topic);
    if (ret==1){
        ESP_LOGI(TAG, "DATA SUB OK");
        // printf("%s\n",msg_sub_topic);
        ret = parse_json_to_alert(msg_sub_topic, data_sub);
        if (ret ==1){
            strcpy(data_sub->mac,ble_mac);
            printf("sub: %s\n",msg_sub_topic);
            //print_data_sub(data_sub);
        }
    }
     
    free(msg_sub_topic);
    return ret;
}


int parse_json_to_alert(const char* json_string, sub_sens_t* data_alert) {
    cJSON *json = cJSON_Parse(json_string);

    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        return 0;
    }

    cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");
    cJSON *ta = cJSON_GetObjectItemCaseSensitive(json, "ta");

    if (!cJSON_IsNumber(id) || !cJSON_IsNumber(ta)) {
        cJSON_Delete(json);
        return 0;
    }

    data_alert->id = id->valueint;
    data_alert->ta = ta->valuedouble;

    cJSON_Delete(json);
    return 1;
}


void data_sens_to_json(data_sens_t data, char* buffer) {
    cJSON *sensor = cJSON_CreateObject();
    cJSON_AddNumberToObject(sensor, "time", (double)data.time);
    cJSON_AddNumberToObject(sensor, "tem", round(data.temperature*1e2)/1e2);
    cJSON_AddNumberToObject(sensor, "hum", round(data.humidity*1e2)/1e2);
    cJSON_AddNumberToObject(sensor, "bat", round(data.battery*1e2)/1e2);
    cJSON_AddNumberToObject(sensor, "id",  data.id);
    cJSON_AddNumberToObject(sensor, "ta",  round(data.temp_alert*1e2)/1e2);
    
    char *json = cJSON_PrintUnformatted(sensor);
    strcpy(buffer, json);
    cJSON_Delete(sensor);
    free(json);
}


int convert_ble_sens_data(sens_ble_t sensor, data_sens_t *data){
    data->battery     = Inkbird_battery(sensor.data_Hx);
    data->temperature = Inkbird_temperature(sensor.data_Hx);
    data->humidity    = Inkbird_humidity(sensor.data_Hx);
    data->time        = sensor.epoch_ts;
    Inkbird_MAC_to_str_1(sensor, data->mac);
    data->id = -1;
    data->temp_alert = 0.0;
    return 0;
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
    strcpy(IMEI,get_M95_IMEI());
    char* fecha_n = get_m95_date();
    time_t fecha_epoch = get_m95_date_epoch();

    ESP_LOGI(TAG,"IMEI: %s", IMEI);
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
    cJSON_AddItemToObject(doc,"imei",cJSON_CreateString(IMEI));
    cJSON_AddItemToObject(doc,"project",cJSON_CreateString(MASTER_TOPIC));
    cJSON_AddItemToObject(doc,"ota",cJSON_CreateString("true"));
    cJSON_AddItemToObject(doc,"cmd",cJSON_CreateString("false"));
    cJSON_AddItemToObject(doc,"sw",cJSON_CreateString(PROJECT_VER));
    cJSON_AddItemToObject(doc,"hw",cJSON_CreateString(PROJECT_VER));
    cJSON_AddItemToObject(doc,"otaV",cJSON_CreateString(PROJECT_VER));
    output = cJSON_PrintUnformatted(doc);

    printf("Mensaje OTA:\r\n");
    printf(output);
    printf("\r\n");

    xTaskCreate(main_task, "main_task", 6*1024, NULL,5, NULL);
}