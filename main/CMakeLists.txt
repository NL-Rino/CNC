# LUU Y: component "main" binh thuong tu dong require TAT CA component cua
# ESP-IDF. Nhung khi khai bao REQUIRES tuong minh thi hanh vi tu dong do BI TAT,
# nen phai liet ke DU cac component dang dung, neu khong se bao loi kieu
# "fatal error: driver/gpio.h: No such file or directory".
idf_component_register(SRCS "main.c"
                        INCLUDE_DIRS "."
                        REQUIRES nvs_flash
                                 esp_driver_gpio
                                 esp_driver_uart
                                 esp_system
                                 esp_common
                                 esp_rom
                                 freertos
                                 log)