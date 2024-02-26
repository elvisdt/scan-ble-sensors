
#include "inkbird_ble.h"
sens_ble_t * disp_ble;


float Inkbird_temperature(const uint8_t * manufacturer_data){
    int16_t temperatura_raw = manufacturer_data[1] * 256 + manufacturer_data[0];
    float temperature = (float)temperatura_raw;
    temperature = temperature/100;
    return temperature;
}


float Inkbird_humidity(const uint8_t * manufacturer_data){
    int16_t humidity_raw_value = (manufacturer_data[3] * 256 + manufacturer_data[2]);
    float humidity = (float)humidity_raw_value;
    humidity = humidity/100;
    return humidity;
}


float Inkbird_battery(const uint8_t * manufacturer_data){
    float battery = manufacturer_data[7];
    return battery;
}

int Inkbird_MAC_to_str(sens_ble_t data,char * buffer){
    char mac_addr[20];
    char hex_number[4];
    memset(mac_addr,0x00,sizeof(mac_addr));
    for (size_t i = 0; i < 6; i++)
    {
        sprintf(hex_number,"%02x:",data.addr[i]);
        strcat(mac_addr,hex_number);
    }
    mac_addr[strlen(mac_addr) - 1] = 0x00;
    strcpy(buffer,mac_addr);
    return 0;
}

int Inkbird_MAC_to_str_1(sens_ble_t data,char * buffer){
    char mac_addr[20];
    char hex_number[4];
    memset(mac_addr,0x00,sizeof(mac_addr));
    for (size_t i = 0; i < 6; i++)
    {
        sprintf(hex_number,"%02x",data.addr[i]);
        strcat(mac_addr,hex_number);
    }
    mac_addr[strlen(mac_addr)] = 0x00;
    strcpy(buffer,mac_addr);
    return 0;
}

void Inkbird_str_to_MAC(char * str_mac,uint8_t * buffer_mac){
    char * pointer;
    pointer = strtok(str_mac,":");
    for (size_t i = 0; i < 6; i++){
        buffer_mac[i] = strtol(pointer,NULL,16);
        pointer  = strtok(NULL,":");
    }
}


int Inkbird_mac_index(sens_ble_t datos_guardados[MAX_BLE_DEVICES],esp_bd_addr_t mac_addr){
    for (int i = 0; i < MAX_BLE_DEVICES; i++)
    {
        if((memcmp(datos_guardados[i].addr,mac_addr,ESP_BD_ADDR_LEN) == 0)&&(Inkbird_check_empty_mac(mac_addr) == false)){
            return i;
        }

    }
    return -1;
}


bool Inkbird_check_empty_mac(esp_bd_addr_t mac_addr){
    uint8_t empty[ESP_BD_ADDR_LEN];
    memset(empty,0x00,ESP_BD_ADDR_LEN);
    if(memcmp(mac_addr,empty,ESP_BD_ADDR_LEN) == 0){
        return true;
    }
    return false;
}

static uint8_t message_extract(char *cadena,char *respuesta,const char inicio[],const char fin[]){
  
  uint8_t control=0;
  uint8_t j=0;

  for(int i=0; i<120;i++){
    switch(control){
    case 0:
        if(cadena[i] == inicio[0]){
            control=1;
        }
        break;
    case 1:
        if(cadena[i] != fin[0]){       
            respuesta[j]=cadena[i];
            j++;
        }else{
            respuesta[j]='\0';
            return 1;
        }
        break;
    default:
        break;
    }
  } 

  return 0;
}

uint8_t Inkbird_cmd(char * cmd,char * buffer_response){
    /* SENSOR */
    /* "SNAME=name,addr,refri;" */
    /* "SNAME=name,addr, ,+;" agregar */
    /* "SNAME=name,addr, ,-;" borrar */
    int longMsg = 0;

    char * message_command = cmd;
    char Respuesta[200];
    char *p_command;
    p_command = strstr((const char *)(message_command + longMsg),"SNAME=");
    //TODO:adaptar para que procese mas de uno
    while(p_command != 0){
        //if (debug) DBGSerial->print("p_command: "); 
        char datos[4][30];
        char mensajeExtraido[40];

        // SEPARAR LOS DOS FACTORES
        if(message_extract(p_command, mensajeExtraido ,"=",";")==1){
            int caracter = 0;
            int dato = 0;
            for(int j=0; j< strlen(mensajeExtraido); j++){
                if(mensajeExtraido[j] == '\0' || j==39 || caracter>39 || dato>3){
                    datos[dato][caracter] = '\0';  
                    break;
                }
                else if(mensajeExtraido[j] == ','){
                    datos[dato][caracter] = '\0';
                    dato++;
                    caracter=0;
                }
                else{
                    datos[dato][caracter] = mensajeExtraido[j];
                    caracter++;
                }    
            } 
            datos[dato][caracter]='\0';     
        }
        printf("DATOS:  \r\n"); 
        printf("%s\r\n",datos[0]);//Name
        printf("%s\r\n",datos[1]);//addr in string 
        printf("%s\r\n",datos[2]);// tipo
        printf("%s\r\n",datos[3]);// "+"
        //Actualizar la ubicacion
        longMsg += strlen(mensajeExtraido);
        //printf("%s\r\n",datos[3][0]);
        esp_bd_addr_t mac_addr;
        Inkbird_str_to_MAC(datos[1],&mac_addr[0]);
        char mac_string[20];
        // FILTRADO
        // hay dos opciones que ya este y que sea nuevo.
        //int posicionDato = 0;
        for(int i=0; i<MAX_BLE_DEVICES;i++){  
            // si encuentra un dato con 00 o si encuentra el Addr igual
            if(datos[3][0] == '+'){
                if( (memcmp(disp_ble[i].addr,mac_addr,ESP_BD_ADDR_LEN) != 0) & Inkbird_check_empty_mac(disp_ble[i].addr)  ){
                    //posicionDato = i;
                    strcpy(disp_ble[i].Name,datos[0]);
                    //sprintf(disp_ble[i].addr,"%s",datos[1]);
                    memcpy(disp_ble[i].addr,mac_addr,ESP_BD_ADDR_LEN);
                    Inkbird_MAC_to_str(disp_ble[i],mac_string);
                    
                    break;
                }      
            }
            else{
                if(memcmp(disp_ble[i].addr,mac_addr,ESP_BD_ADDR_LEN) == 0){
                    
                    sprintf(Respuesta+strlen(Respuesta),"Name: %s Addr: %s Tipo: %s BORRADO; \n", disp_ble[i].Name, datos[1], "cooler"); 

                    memset(disp_ble[i].Name,0x00,sizeof(((sens_ble_t *)0)->Name));
                    memset(disp_ble[i].addr,0x00,sizeof(((sens_ble_t *)0)->addr));                    
                    break;
                }
            }  
            }
        p_command=strstr((const char *)(message_command+7+longMsg),"SNAME=");   
        }
    strcpy(buffer_response,Respuesta);
    return 0;
}

void Inkbird_register_devices_buffer(sens_ble_t  * devices){
    disp_ble = devices;
}


//-----------------------------------------------//

void print_sensor_data(sens_ble_t* devices) {
    printf("Nombre: %s\n", devices->Name);

    printf("Dirección: ");
    for (int i = 0; i < sizeof(devices->addr); i++) {
        printf("%02x:", devices->addr[i]);
    }
    printf("\n");

    printf("Datos Hex: ");
    for (int i = 0; i < sizeof(devices->data_Hx); i++) {
        printf("%02x ", devices->data_Hx[i]);
    }
    printf("\n");

    printf("Marca de tiempo: %lld\n", devices->epoch_ts);
}


