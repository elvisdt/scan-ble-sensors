/**
 * @file inkbird_ble.h
 * @brief This file contains the declarations for functions and structures
 *        related to Inkbird Bluetooth Low Energy (BLE) devices.
 */

#ifndef INKBIRD_BLE_H
#define INKBIRD_BLE_H

#include <stdint.h>
#include <esp_bt_defs.h>
#include <stdio.h>
#include <string.h>


#define MAX_BLE_DEVICES 10

typedef struct{
    char    Name[10];
    esp_bd_addr_t addr;
    uint8_t data_Hx[9];
    time_t  epoch_ts;
}sens_ble_t;


typedef struct {
    char     mac[20];
    float    temperature;
    float    humidity;
    float    battery;
    time_t   time;
}data_sens_t;



typedef struct {
    char name[50];
    char phone[15];
} sub_user_t;

typedef struct {
    char mac[20];
    int id;
    float tem;
} sub_sens_t;

typedef struct {
    sub_user_t user;
    sub_sens_t sens[MAX_BLE_DEVICES];
    int sens_count;
} sub_data_t;



/**
 * @brief Registers the buffer for storing device information.
 *
 * This function registers the buffer to store Inkbird device information.
 *
 * @param devices Pointer to the buffer for storing device information.
 */
void Inkbird_register_devices_buffer(sens_ble_t *devices);



/**
 * @brief Finds the index of a device by MAC address.
 *
 * This function searches for a device with a specific MAC address within
 * the given array of devices.
 *
 * @param datos_guardados Array of stored device information.
 * @param mac_addr MAC address to search for.
 * @return Index of the device in the array, or -1 if not found.
 */
int Inkbird_mac_index(sens_ble_t datos_guardados[MAX_BLE_DEVICES], esp_bd_addr_t mac_addr);



/**
 * @brief Extracts temperature from manufacturer data.
 *
 * This function extracts temperature information from the manufacturer data
 * received from an Inkbird BLE device.
 *
 * @param manufacturer_data Pointer to the manufacturer data.
 * @return Temperature value in degrees Celsius.
 */
float Inkbird_temperature(const uint8_t *manufacturer_data);



/**
 * @brief Extracts humidity from manufacturer data.
 *
 * This function extracts humidity information from the manufacturer data
 * received from an Inkbird BLE device.
 *
 * @param manufacturer_data Pointer to the manufacturer data.
 * @return Humidity value in percentage.
 */
float Inkbird_humidity(const uint8_t *manufacturer_data);



/**
 * @brief Extracts battery level from manufacturer data.
 *
 * This function extracts battery level information from the manufacturer data
 * received from an Inkbird BLE device.
 *
 * @param manufacturer_data Pointer to the manufacturer data.
 * @return Battery level in percentage.
 */
float Inkbird_battery(const uint8_t *manufacturer_data);



/**
 * @brief Formats device sample information into a string.
 *
 * This function formats temperature, humidity, battery level, and timestamp
 * information from an Inkbird BLE device sample into a string.
 *
 * @param sample Pointer to the Inkbird sample.
 * @param buffer Pointer to the output buffer to store the formatted string.
 * @return Pointer to the formatted string.
 */
//char *Inkbird_sample_info(inkbird_sample *sample, char *buffer);



/**
 * @brief Formats device information into a string.
 *
 * This function formats device name and MAC address into a string.
 *
 * @param sample Pointer to the Inkbird device information.
 * @param buffer Pointer to the output buffer to store the formatted string.
 * @return Pointer to the formatted string.
 */
char *Inkbird_device_info(sens_ble_t *sample, char *buffer);



/**
 * @brief Checks if a MAC address is empty.
 *
 * This function checks if a MAC address is empty (all zeros).
 *
 * @param mac_addr MAC address to check.
 * @return True if the MAC address is empty, false otherwise.
 */
bool Inkbird_check_empty_mac(esp_bd_addr_t mac_addr);



/**
 * @brief Converts MAC address from bytes to a string.
 *
 * This function converts a MAC address stored as bytes in sens_ble_t to
 * a string representation.
 *
 * @param data Pointer to the sens_ble_t structure.
 * @param buffer Pointer to the output buffer to store the MAC address string.
 * @return 0 if successful.
 */
int Inkbird_MAC_to_str(sens_ble_t data, char *buffer);



/**
 * @brief Converts MAC address from string to bytes.
 *
 * This function converts a MAC address stored as a string to bytes.
 *
 * @param str_mac Pointer to the MAC address string.
 * @param buffer_mac Pointer to the output buffer to store the MAC address bytes.
 */
void Inkbird_str_to_MAC(char *str_mac, uint8_t *buffer_mac);



/**
 * @brief Processes a command and updates device information.
 *
 * This function processes a command string to update device information.
 *
 * @param cmd Pointer to the command string.
 * @param buffer_response Pointer to the output buffer to store the response.
 * @return 0 if successful.
 */
uint8_t Inkbird_cmd(char *cmd, char *buffer_response);

/**
 * @brief Imprime los datos de un sensor.
 *
 * Esta función toma un puntero a una estructura sens_ble_t y 
 * luego imprime cada uno de los campos. Para los campos que son 
 * arrays de bytes (addr y data_Hx), se imprime cada byte en formato hexadecimal.
 *
 * @param device Un puntero a una estructura sens_ble_t que contiene los datos del sensor.
 */
void print_sensor_data(sens_ble_t* devices);

//char *Inkbird_trama_Mqtt(inkbird_sample *data, char *buffer);

#endif // INKBIRD_BLE_H
