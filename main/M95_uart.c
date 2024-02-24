#include "M95_uart.h"
#include "esp32_general.h"
#include "credenciales.h"

#define BUF_SIZE_MODEM                  (1024)
#define RD_BUF_SIZE                     (BUF_SIZE_MODEM)

int debug = 0;

esp_ota_handle_t otaHandlerXXX = 0;


// M95 buffer para guardar datos recibidos por el M95
//char 		buff_reciv[255];
char 		buff_reciv[500];
//char* 		uart_m95_string;
// Bandera para sincronizar hora
uint8_t  	epoch_ready;

// buffer para almacenar respuesta M95 despues de publicar en MQTT
//uint8_t  	buffer_m95[128];
uint64_t 	actual_time_M95;
uint64_t 	idle_time_m95;

#define BUFFER_SIZE 5140
char buffer[BUFFER_SIZE];

// String para los comandos que se enviaran al M95
char buff_send[100];

int M95_check_uart(char* ok_msg, int time){
	// Enviamos el commando AT
	char* com = "AT\r\n";

	// Comando para iniciar comunicacion
	rx_modem_ready = 0;
	uart_write_bytes(UART_MODEM, (uint8_t *)com, strlen(com));
	
	// Intentos
	int a = 3;
	ESP_LOGI("M95","Espernado respuesta ...\n ");

	//rx_modem_ready = 0;
	//uart_write_bytes(UART_MODEM, (uint8_t *)com, strlen(com));
	debug = 0;

	while(a > 0){

		if( ( readAT(ok_msg,"ERROR",time*1000,buff_reciv) == 1 ) ){
			printf("Already active M95\n\r");
			break;
		}
		printf(".-.-.\n\r");
		vTaskDelay(300 / portTICK_PERIOD_MS);
		a--;
	}
	printf("\r\n");
	debug = 1;

	return a;
}


/** 
    @brief Encendemos el equipo mediante el PWR_PIN del M95
 */
void M95_pwron(){
    activate_pin(PWRKEY_Pin); 		
	vTaskDelay(1000 / portTICK_PERIOD_MS);
    
	deactivate_pin(PWRKEY_Pin); 	
	vTaskDelay(2000 / portTICK_PERIOD_MS);

	actual_time_M95 = esp_timer_get_time();

	while(gpio_get_level(STATUS_Pin) == 0)
	{
		if( ( esp_timer_get_time() - actual_time_M95 ) > S_TO_US(5)) break;
		vTaskDelay(300 / portTICK_PERIOD_MS);
	}

	
	if(M95_check_uart("Call Ready",3) < 1){
		ESP_LOGE("FATAL ERROR","M95 NO RESPONDE, REINICIANDO ...");
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		esp_restart();
	}
	uart_flush(UART_MODEM);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
}

/** 
    @brief Apagamos el equipo mediante el PWR_PIN del M95
 */
void M95_poweroff(){
	printf("Apagando el modem ....\n");
	deactivate_pin(PWRKEY_Pin);
	vTaskDelay(1500 / portTICK_PERIOD_MS);

	while( gpio_get_level( STATUS_Pin ) == 1 ){
		activate_pin(PWRKEY_Pin); 		
		vTaskDelay(800 / portTICK_PERIOD_MS);
		deactivate_pin(PWRKEY_Pin);

		while(gpio_get_level(STATUS_Pin) == 1){
			vTaskDelay(1500 / portTICK_PERIOD_MS);
			printf("... \n");
		}
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}


void M95_checkpower(){

    if( gpio_get_level( STATUS_Pin ) == 1 ){
		printf("Power ON detected: \n\r");
		if( M95_check_uart("OK",3) > 0 ){
			return ; 
		}
		printf("\t ... turning off ... \n\r");
        activate_pin( LED_READY );
        M95_poweroff();
    }
	
	vTaskDelay(1000 / portTICK_PERIOD_MS);

    if( gpio_get_level( STATUS_Pin ) == 0 ){
		printf("Encendiendo Modem\n\r");
        M95_pwron();
		deactivate_pin( LED_READY );
    }

}

/** 
    @brief  Apagamos el equipo mediante UART y el Comando AT "POWER OFF"
			correspondiente
 */
int M95_poweroff_command(){
	if(sendAT("AT+QPOWD=0\r\n","OK\r\n","FAILED\r\n",2000,buff_reciv) != 1){
		return sendAT("AT+QPOWD=1\r\n","OK\r\n","FAILED\r\n",2000,buff_reciv);
	}
	return 1;
}

/** 
    @brief  Obtenemos el IMEI del M95 mediante comando AT
 */
char* get_M95_IMEI(){
	// buff_reciv -> Variable donde se almacena la respuesta del M95
	int intentos = 4;
	while(intentos > 0){
		printf("Obteniendo IMEI ... \n");
		if(sendAT("\r\nAT+QGSN\r\n","+QGSN","ERROR",2000,buff_reciv) == 1){
			//printf("1- RESPUESTA =%s\n",(char*)buff_reciv);
			break;
		}
		//printf("2- RESPUESTA =%s\n",(char*)buff_reciv);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		intentos--;
	}
	if(intentos == 0){
		//printf("ERROR: No se obtuvo IMEI");
		return "NULL";
	}

	//printf("3- RESPUESTA =%s\n",(char*)buff_reciv);
	char lim[3] = "\"";		// lim = "
	char * p_clock = strtok(buff_reciv,lim);
	while(p_clock != NULL){
		if(strstr(p_clock,"+")==NULL && strstr(p_clock,":") == NULL){
			break;
		}
		p_clock=strtok(NULL,lim);
	}
	return p_clock;
}

/** 
    @brief  Obtenemos la calidad de señal del M95 mediante comando AT
 */
int get_M95_signal(){
	// buff_reciv -> Variable donde se almacena la respuesta del M95
	int intentos = 4;
	while(intentos > 0){
		if(sendAT("\r\nAT+CSQ\r\n","+CSQ","ERROR",500,buff_reciv) == 1){
			break;
		}
		intentos--;
	}
	if(intentos == 0){
		printf("No responde el M95");
		return 99;
	}

	//printf("RESPUESTA =%s\n",(char*)buff_reciv);
	char lim[3] = ":, ";		// lim = "
	char * p_clock = strtok(buff_reciv,lim);
	while(p_clock != NULL){
		if(strstr(p_clock,"CSQ")!=NULL){
			break;
		}
		p_clock=strtok(NULL,lim);
	}
	p_clock=strtok(NULL,lim);

	return atoi(p_clock);
}

/** 
    @brief Genera el checksum del protocolo rs485 - en base al
            array de elementos 0xXX; /data_b y de tamaño /size_b
            y guarda esos valores en /result  
 */
int M95_PubMqtt_data(uint8_t * data,char * topic,uint16_t data_len,uint8_t tcpconnectID){
	int k = 0;
	sprintf(buff_send,"AT+QMTPUB=%d,0,0,1,\"%s\",%d\r\n",tcpconnectID,topic,data_len);
	//printf("Pub_msg=\n%s\n",buff_send);
	
	sendAT(buff_send,">","ERROR",1000,buff_reciv);
	vTaskDelay(10);

	uart_write_bytes(UART_MODEM,data,data_len);
	k = readAT("+QMT","ERROR\r\n",5000,buff_reciv);
	if(k != 1) return 0;
	return 1;
}


int M95_SubTopic(int tcpconnectID, char* topic_name, char* response){
    sprintf(buff_send,"AT+QMTSUB=%d,1,\"%s\",0\r\n",tcpconnectID,topic_name);
    int success = 0;    
    if(sendAT(buff_send,"+QMTRECV:","ERROR\r\n",20000,buff_reciv) == 1){
        success = 1;
    }

    if(success == 0){
        ESP_LOGE("MQTT Subs","No se recibio respuesta del topico:\n%s\n",topic_name);
        return 0;
    }
	
	char *start;
	start = strchr(buff_reciv, '{');
	// Si se encontró la llave
	if(start != NULL){

		char *data = strdup(start);
		strcpy(response,data);

		free(data);
		return 1; // RESPUESTA OK
	}
    return -1;
}

uint8_t M95_begin(){
    int bandera=5;
	debug = 0;

	// Disable local echo mode			
	sendAT("ATE0\r\n","OK\r\n","ERROR\r\n",5000,buff_reciv);		
	deactivate_pin(BLUE_LED);
	vTaskDelay(500 / portTICK_PERIOD_MS);

	// Disable "result code" prefix
	sendAT("AT+CMEE=0\r\n","OK\r\n","ERROR\r\n",5000,buff_reciv);	
	activate_pin(BLUE_LED);
	vTaskDelay(500 / portTICK_PERIOD_MS);

	// Indicate if a password is required
	sendAT("AT+CPIN?\r\n","OK\r\n","ERROR\r\n", 2000,buff_reciv);
	vTaskDelay(500 / portTICK_PERIOD_MS);

	sendAT("ATI\r\n","OK","ERROR\r\n", 500,buff_reciv);
	vTaskDelay(500 / portTICK_PERIOD_MS);

	// Set SMS message format as text mode	
	sendAT("AT+CMGF=1\r\n","OK\r\n","ERROR\r\n", 300,buff_reciv);
	vTaskDelay(500 / portTICK_PERIOD_MS);


	// Signal Quality - .+CSQ: 6,0  muy bajo
	sendAT("AT+CSQ\r\n","CSQ","ERROR\r\n", 300,buff_reciv);
	vTaskDelay(200 / portTICK_PERIOD_MS);

	// Network Registration Status	
	bandera=sendAT("AT+CGREG?\r\n","0,","ERROR\r\n", 300, buff_reciv);
	vTaskDelay(200 / portTICK_PERIOD_MS);

	// Request IMEI (Internatinal Mobile Identify)
	sendAT("AT+GSN\r\n","OK\r\n","ERROR\r\n", 300, buff_reciv);
	vTaskDelay(200 / portTICK_PERIOD_MS);

	//Delete all SMS
	sendAT("AT+QMGDA=\"DEL ALL\"\r\n","OK\r\n","ERROR\r\n", 10000, buff_reciv);
	vTaskDelay(200 / portTICK_PERIOD_MS);

	// Activate GPRS context
	sendAT("AT+QIACT\r\n","OK\r\n","ERROR\r\n" , 15000, buff_reciv);
	vTaskDelay(500/portTICK_PERIOD_MS);

	// Synchronize the Local Time Via NTP
	printf("Actualizando la hora\n\r");
	bandera = sendAT("AT+QNTP=\"pool.ntp.org\"\r\n"
					,"+QNTP: 0","3",12000,buff_reciv);
	if((bandera == 0)){
		printf("No se pudo actualizar la hora\n\r");
		vTaskDelay(100/portTICK_PERIOD_MS);
	}
	else{
		printf("Hora Actualizada\n\rEquipo Inicializado Correctamente \n\r");
		vTaskDelay(200 / portTICK_PERIOD_MS);
	}

	activate_pin(LED_READY);
	return bandera;
}

int connect_MQTT_server(int id){

	char respuesta_esperada[30];
	//sprintf(buff_send,"AT+QMTCFG=\"TIMEOUT\",%d,10,3,0\r\n",id);
	//sprintf(buff_send,"AT+QMTCFG=\"recv/mode\",%d\r\n",id);
	//sendAT(buff_send,"OK","ERROR\r\n",500,buff_reciv);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	
	sprintf(buff_send,"AT+QMTOPEN=%d,\"%s\",%s\r\n",id,ip_MQTT,port_MQTT);
	sprintf(respuesta_esperada,"+QMTOPEN: %d,0",id);
	//printf("\n- Connect = %s\n",buff_send);
	
	printf("MQTT: Abriendo conexion...\r\n");

	if(sendAT(buff_send,respuesta_esperada,"ERROR",70000,buff_reciv) != 1){
		ESP_LOGE("QMTOPEN","Bad Syntaxis");
		return 0;
	}
	if(strstr((char*)buff_reciv,"-1")){
		ESP_LOGE("M95 E","Cant open IP");
		return 0;
	}
	vTaskDelay(1000 / portTICK_PERIOD_MS);

    // Connect with name "M95" to Broker
	sprintf(buff_send,"AT+QMTCONN=%d,\"%s\"\r\n",id,CLIENT_ID_MQTT);
	sprintf(respuesta_esperada,"+QMTCONN: %d,0",id);

	int a = sendAT(buff_send,respuesta_esperada,"ERROR\r\n",5000,buff_reciv);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	return a;
}

int M95_CheckConnection(int id){	
	return connect_MQTT_server(id);
}

int m95_sub_topic_json(int ID, char* topic_name, char* response){
	sprintf(buff_send,"AT+QMTSUB=%d,2,\"%s\",0\r\n",ID,topic_name);
	int success = 0;	
	if(sendAT(buff_send,"+QMTRECV:","ERROR\r\n",10000,buff_reciv) == 1){
		success = 1;
	}

	if(success == 0){
		deactivate_pin(LED_READY);
		ESP_LOGE("Suscripcion","No se recibio respuesta del topico:\n%s\n",topic_name);
		return -1;
	}
	
	char lim[3] = "{}";
	char * p_clock = strtok((char*)buff_reciv,topic_name);
	//printf("Iteracion = |%s|\n",p_clock);
	
	while(p_clock != NULL){
		if(strstr(p_clock,"max")!= NULL){
			break;
		}
		p_clock=strtok(NULL,lim);
		//printf("Iteracion = |%s|\n",p_clock);
	}
	strcpy(response,p_clock);
	return 1;
}

int m95_sub_topic_OTA(int ID, char* topic_OTA, char* response){

	sprintf(buff_send,"AT+QMTSUB=%d,2,\"%s\",0\r\n",ID,topic_OTA);
	int success = 0;	
	if(sendAT(buff_send,"+QMTRECV:","ERROR\r\n",10000,buff_reciv) == 1){
		success = 1;
	}
	if(success == 0){
		deactivate_pin(LED_READY);
		ESP_LOGE("Suscripcion","No se recibio respuesta del topico:\n%s\n",topic_OTA);
		return -1;
	}

	char lim[2] = ",";
	char * p_clock = strtok((char*)buff_reciv,lim);
	//printf("Iteracion = |%s|\n",p_clock);
	
	while(p_clock != NULL){
		if(strstr(p_clock,"OTA")!= NULL){
			p_clock=strtok(NULL,lim);
			break;
		}
		p_clock=strtok(NULL,lim);
		//printf("Iteracion = |%s|\n",p_clock);
	}
	//printf("Resultado =\n%s\n",p_clock);
	strcpy(response,p_clock);
	return 1;
}


void m95_unsub_topic(int ID, char* topic_name){
	sprintf(buff_send,"AT+QMTUNS=%d,3,\"%s\"",ID,topic_name);
	sendAT(buff_send,"+QMTUNS:","ERROR\r\n",7000,buff_reciv);
}

char* get_m95_date(){
	int a = sendAT("AT+CCLK?\r\n","+CCLK: ","ERROR",1000,buff_reciv);
	if( a != 1){
		printf("hora no conseguida\n\r");	
		return NULL;		
	}

	char lim[3] = "\"";
	char * p_clock = strtok(buff_reciv,lim);
	while(p_clock != NULL){
		if(strstr(p_clock,"/")!= NULL){
			break;
		}
		p_clock = strtok(NULL,lim);
	}
	return (char*) p_clock;
}



time_t get_m95_date_epoch() {

	char *date_str =  get_m95_date();
	if (date_str == NULL) {
		return -1;	
    } 

	struct tm timeinfo = { 0 };
    int year, month, day, hour, minute, second;

    // Parsea la fecha y hora de la cadena de texto
    sscanf(date_str, "%2d/%2d/%2d,%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second);

    timeinfo.tm_year = year + 100; 	// Los años se cuentan desde 1900
    timeinfo.tm_mon = month - 1; 	// Los meses se cuentan desde 0
    timeinfo.tm_mday = day;		 
    timeinfo.tm_hour = hour;	// La diferencia de Horas de formato UTC global
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;

    // Convierte struct tm a tiempo epoch
    time_t epoch_time = mktime(&timeinfo);

	struct timeval stv;
	stv.tv_sec = epoch_time;
	stv.tv_usec = 0;
	settimeofday(&stv, NULL);
	
	return epoch_time;
}



int sendAT(char *command, char *ok, char *error, uint32_t timeout, char *response)
{
	memset(response, '\0',strlen(response));
	uint8_t send_resultado = 0;
	//Define "UART_MODEM" in program (i.e. #define UART_MODEM UART_NUM_2)
	actual_time_M95 = esp_timer_get_time();
	idle_time_m95 = (uint64_t)(timeout*1000);

	debug = 0;

	// printf("Send AT esperando: %lu ms\n\r",timeout);
	rx_modem_ready = 0;
	uart_write_bytes(UART_MODEM, (uint8_t *)command, strlen(command));

	while( ( esp_timer_get_time() - actual_time_M95 ) < idle_time_m95 ){
		if( rx_modem_ready == 0 ){
			vTaskDelay( 10 / portTICK_PERIOD_MS );
			continue;
		}
		if ( strstr((char *)p_RxModem,ok) != NULL ){
			send_resultado = 1;     
			break;
		}
		else if( strstr((char *)p_RxModem,error) != NULL ){
			send_resultado = 2;
			break;
		}
		rx_modem_ready = 0;
	}


    if( send_resultado == 0 ){
		ESP_LOGE("M95 error","\nTIMEOUT\n");
        return 0;
    }

	//memcpy(response,p_RxModem,rxBytesModem);
	strcpy(response,(char*)p_RxModem);

	if( debug == 1 ){
		ESP_LOGI("M95_tag","%s\n",command);
		if(send_resultado == 1){
			ESP_LOGI("M95 sucess","\n[%s]\n",response);
		}
		else {
			ESP_LOGE("M95 error","\n[%s]\n",response);
		}
	}
	return send_resultado;
}

int readAT(char *ok, char *error, uint32_t timeout, char *response)
{
	//memset(response, '\0',strlen(response));
	int correcto = 0;
	bool _timeout = true;

	actual_time_M95 = esp_timer_get_time();
	idle_time_m95 = (uint64_t)(timeout*1000);

	rx_modem_ready = 0;

	while((esp_timer_get_time() - actual_time_M95) < idle_time_m95){
		if(rx_modem_ready == 0){
			vTaskDelay( 1 / portTICK_PERIOD_MS );
			continue;
		}
		if (strstr((char *)p_RxModem,ok) != NULL){
			correcto = 1;
			_timeout = false;
			break;
		}
		else if(strstr((char *)p_RxModem,error)!= NULL){
			correcto = 2;
			_timeout = false;
			break;
		}
		vTaskDelay( 5 / portTICK_PERIOD_MS );
		rx_modem_ready = 0;
	}	

    if(_timeout){
		printf("- M95 not responded\n");
        return 0;
    }

	//memcpy(response,p_RxModem,rxBytesModem);
	strcpy(response,(char*)p_RxModem);
	if(debug==1){
		printf(response);
		printf("\r\n");
	}

	return correcto;
}

void disconnect_mqtt(){
	//sendAT("AT+QMTDISC=1\r\n","OK\r\n", "ERROR\r\n", 1000, buff_reciv);
	//vTaskDelay(1000 / portTICK_PERIOD_MS);
	sendAT("AT+QMTCLOSE=0\r\n","OK\r\n","ERROR\r\n", 1000,buff_reciv);
	vTaskDelay(1000/portTICK_PERIOD_MS);
}



/*
uint8_t OTA(uint8_t *buff, uint8_t *inicio, uint8_t *fin, uint32_t len){
    const char *TAG = "OTA";
    if (*inicio) { //If it's the first packet of OTA since bootup, begin OTA
        ESP_LOGI(TAG,"BeginOTA");
        const esp_task_wdt_config_t config_wd = {
            .timeout_ms = 50,
            .idle_core_mask = 0,
            .trigger_panic = false,
        };
        
        esp_task_wdt_init(&config_wd);
        esp_ota_begin(esp_ota_get_next_update_partition(NULL), OTA_SIZE_UNKNOWN, &otaHandlerXXX);
        *inicio = 0;
    }
    if (len > 0)
    {
        esp_ota_write(otaHandlerXXX,(const void *)buff, len);

        if (len != 512 || *fin ==1)
        {
            esp_ota_end(otaHandlerXXX);
			const esp_task_wdt_config_t config_wd = {
				.timeout_ms = 5,
				.idle_core_mask = 0,
				.trigger_panic = false,
			};
			
			esp_task_wdt_init(&config_wd);
            ESP_LOGI(TAG,"EndOTA");
            //Serial.println("EndOTA");
            if (ESP_OK == esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL))) {
                vTaskDelay(2000 / portTICK_PERIOD_MS);
                esp_restart();
            }
            else {
                ESP_LOGI(TAG,"Upload Error");
                //Serial.println("Upload Error");
            }
        }

    }

    //uint8_t txData[5] = {1, 2, 3, 4, 5};
    //delay(1000);
    return 1;
}

char M95_readSMS(char* sms_tmp){
	memset(sms_tmp,'\0',strlen(sms_tmp));
	if(sendAT("AT+CMGL=\"REC UNREAD\"\r\n","+CMGL","ERROR\r\n",5000,sms_tmp)==1){
		printf("SMS: SMS encontrado\r\n");
		return 1;
	} else{
		return 0;
	}
}


void M95_sendSMS(char *mensaje, char *numero){
	char temp_resp[200];
	memset(buffer, '\0',strlen(buffer));
	sprintf(buffer,"AT+CMGS=\"%s\"\r\n",numero);
	sendAT(buffer,">", "ERROR\r\n", 5000,temp_resp);

	memset(buffer, '\0',strlen(buffer));
	sprintf(buffer,"%s%c",mensaje,26);
	uart_flush(UART_MODEM);
	uart_write_bytes(UART_MODEM,buffer,strlen(buffer));
	uart_wait_tx_done(UART_MODEM,100000/portTICK_PERIOD_MS);
	readAT("+CMGS","ERROR",10000,temp_resp);
}
*/

uint8_t TCP_open(){
    uint8_t temporal=0;
	//debug = 0;
	// Select a Context as Foreground Context : = 0 VIRTUAL_UART_1  := 1 VIRTUAL_UART_2 
    sendAT("AT+QIFGCNT=0\r\n","OK","ERROR",10000,buff_reciv);
	vTaskDelay(50 / portTICK_PERIOD_MS);



    // Control Whether or Not to Enable Multiple TCPIP Session: 0 Simple - 1 Multiple
    sendAT("AT+QIMUX=1\r\n","OK","ERROR",10000,buff_reciv);
    vTaskDelay(50 / portTICK_PERIOD_MS);

    // Transparente 0 no trasnparente 1 transparente
	// Select TCPIP Transfer Mode
    sendAT("AT+QIMODE=0\r\n","OK","ERROR",10000,buff_reciv);
	vTaskDelay(50 / portTICK_PERIOD_MS);

    //IMPORTANTE: LEER ESTE COMANDO
    ///enviar_AT("AT+QITCFG=3,2,512,1\r\n","K\r\n","ERROR\r\n",25500,   buff_reciv);


	// Select CSD or GPRS as the Bearer
	sprintf(buff_send,"AT+QIREGAPP=\"%s\",\"\",\"\"\r\n",SIM_APN);
	sendAT(buff_send,"OK","ERROR",10000,buff_reciv);
	vTaskDelay(50 / portTICK_PERIOD_MS);

    // register TCP/IP  stack
	// AT+QIREGAPP Start TCPIP Task and Set APN, User Name and Password
    sendAT("AT+QIREGAPP\r\n","OK","ERROR",25500,buff_reciv);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
	
    // USE ip address as the address to establish TCP/UDP
	// Connect with IP Address or Domain Name Server
    sendAT("AT+QIDNSIP=0\r\n","OK","ERROR",25500,buff_reciv);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // Activate FGCNT
	// Activate GPRS/CSD Context
    // sendAT("AT+QIACT\r\n","OK","ERROR",25500,buff_reciv);
    // vTaskDelay(2000 / portTICK_PERIOD_MS);

    // Get Local IP address
    sendAT("AT+QILOCIP\r\n",".","ERROR",35000,buff_reciv);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    sprintf(buff_send,"AT+QIOPEN=\"TCP\",\"%s\",%s",ip_OTA,port_OTA);
	printf("OTA IP =\n%s\n",buff_send);
    uart_write_bytes(UART_MODEM,buff_send,strlen(buff_send));

    sendAT("\"\r\n","OK","ERROR",5000,buff_reciv);
    temporal = readAT("CONNECT OK","ERROR",10000,buff_reciv);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
    return temporal; //1 == ok 1 != error
}

uint8_t TCP_send(char *msg, uint8_t len){
    //sendAT(m95,"ATE1\r\n","OK\r\n","ERROR\r\n",5000,  m95->buff_reciv);//DES Activa modo ECO.

    uint8_t temporal = 0;

	debug = 0;
	//debug = 1;
    temporal = sendAT("AT+QISEND\r\n",">","ERROR\r\n",25500,buff_reciv);
    if(temporal != 1){ 
        return 0; 
    }
	uart_write_bytes(UART_MODEM,(void *)msg,len);

    temporal = sendAT("\x1A","SEND OK\r\n","ERROR\r\n",25500,buff_reciv);
	if(temporal != 1){
        return 0;
    }
    //printf("SEND OK CORRECTO\r\n");
	memset(buff_reciv, 0 , 50);
    return 1;
}

void TCP_close(){
	vTaskDelay(200 / portTICK_PERIOD_MS);          
    sendAT("AT+QICLOSE\r\n","OK\r\n","ERROR\r\n",5000,buff_reciv);
    sendAT("AT+QIDEACT\r\n","OK\r\n","ERROR\r\n",5000,buff_reciv);
}

void reiniciar(){
    //encender();

    M95_pwron();
    uint8_t fin = 0;
    const char * TAG = "Reinciar";
    do{
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        if(sendAT("AT\r\n","OK\r\n","ERROR\r\n",5000,buff_reciv)){
            fin=1;
            ESP_LOGI(TAG,"M95 respondio, encendido");
            //DBGSerial->println("M95 respondio, encendido");
        }
        else{
            ESP_LOGI(TAG,"No respondio el M95... encendiendo....");
            //DBGSerial->println("No respondio el M95... encendiendo....");
            //encender();
            M95_pwron();
        }

    }while(!fin);

    //delay(30000);
    vTaskDelay(30000 / portTICK_PERIOD_MS);
    M95_begin();
    vTaskDelay(20000 / portTICK_PERIOD_MS);
}


/*************************************
 * SMS
**************************************/
//--------------- SMS---------------------//
int M95_send_SMS(char *phone, char *msg, int data_len){
	int ret =0; 
	ret = sendAT("AT+CMGF=1\r\n","OK\r\n","ERROR\r\n",5000,buff_reciv);
	vTaskDelay(pdMS_TO_TICKS(100));
	ret = sendAT("AT+CSCS=\"GSM\"\r\n","OK\r\n","ERROR\r\n",5000,buff_reciv);
	vTaskDelay(pdMS_TO_TICKS(100));

	// sprintf(buff_send,"AT+CMGS=\"%s\"\r\n",phone);
	sprintf(buff_send,"AT+CMGS=%s\r\n",phone);
	//printf("Pub_msg=\n%s\n",buff_send);
	ret = sendAT(buff_send,">","ERROR",10000,buff_reciv);
	vTaskDelay(pdMS_TO_TICKS(500));

	// sendAT("hola\r\n","+CMGS","ERROR",1000,buff_reciv);
	uart_write_bytes(UART_MODEM,(uint8_t *)msg, data_len);
	vTaskDelay(pdMS_TO_TICKS(50));
	char ctrl_z = '\x1A';
	uart_write_bytes(UART_MODEM,&ctrl_z, 1);

	ret = readAT("+CMGS","ERROR",1000,buff_reciv);
	return ret;
}
