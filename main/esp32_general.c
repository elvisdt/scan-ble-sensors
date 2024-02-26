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




void remove_newlines(char* str) {
    char* p;
    while ((p = strchr(str, '\n')) != NULL) {
        *p = ' ';  // Reemplaza el salto de línea con un espacio
    }
    while ((p = strchr(str, '\r')) != NULL) {
        memmove(p, p + 1, strlen(p));
    }
}

char* get_middle_part(char* str) {
    char* start = strstr(str, "\" ");
    char* end = strstr(str, " OK");
    char* middle = NULL;  // Declara 'middle' aquí
    
    if(start != NULL && end != NULL) {
        start += 2;  // Avanza el puntero 'start' dos posiciones
        size_t length = end - start;
        middle = malloc(length + 1);
        strncpy(middle, start, length);
        middle[length] = '\0';  // Asegura que la cadena esté terminada en NULL
        // middle = trim(middle);
        // printf("midle: %s\r\n",middle);
    }

    return middle;
}

split_result_t split_msg(char* msg){
    char** parts = malloc(SMS_MAX_PARTS * sizeof(char*));
    char *token;
    int i = 0;
    remove_newlines(msg);
    token = strtok(msg, ":");
    if(token != NULL) {
        parts[i] = malloc(strlen(token) + 1);
        strcpy(parts[i], token);
        i++;
        
        token = strtok(NULL, ",");
        while(token != NULL && i < SMS_MAX_PARTS) {
            parts[i] = malloc(strlen(token) + 1);
            strcpy(parts[i], token);
            i++;
            token = strtok(NULL, ",");
        }
    }

    split_result_t result = {parts, i};
    return result;
}