
cd ..

pio run -e lms-esp32_ili9341_lvgl
pio run -t buildfs -e lms-esp32_ili9341_lvgl
pio run -e lms-esp32_st7789_lvgl
pio run -t buildfs -e lms-esp32_st7789_lvgl
pio run -e cydc-lvgl
pio run -t buildfs -e cydc-lvgl
pio run -e cydr-lvgl
pio run -t buildfs -e cydr-lvgl
pio run -e cydr-usb2-lvgl
pio run -t buildfs -e cydr-usb2-lvgl
pio run -e m5core2-lvgl
pio run -t buildfs -e m5core2-lvgl
pio run -e iot-bus-fixed-lvgl
pio run -t buildfs -e iot-bus-fixed-lvgl

cd firmware_single_file
call make_all v351