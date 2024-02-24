

#------MAIN---------#
set(srcs 
    "main.c"
    "M95_uart.c"
    "ota_m95.c"
    "esp32_general.c"
    "inkbird_ble.c"
    )


#-----------ota-----------------#
set(ota_src 
    "ota/crc.c"
    "ota/ota_control.c"
    "ota/ota_esp32.c"
    "ota/ota_global.c"
    "ota/ota_headers.c"
    )

idf_component_register(
    SRCS ${srcs} ${ota_srcs}
    INCLUDE_DIRS "." "ota"
)