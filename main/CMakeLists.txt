

#------MAIN---------#
set(srcs 
    "main.c"
    "M95_uart.c"
    "ota_m95.c"
    "esp32_general.c"
    "inkbird_ble.c"

    "crc.c"
    "ota_control.c"
    "ota_esp32.c"
    "ota_global.c"
    "ota_headers.c"
    )


#-----------ota-----------------#

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "." "inc"
)