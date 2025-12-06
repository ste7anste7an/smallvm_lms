
cd ..

pio run -e lms-esp32_ili9341_lvgl
pio run -t buildfs -e lms-esp32_ili9341_lvgl
pio run -e lms-esp32_st7789_lvgl
pio run -t buildfs -e lms-esp32_st7789_lvgl

cd firmware_single_file
call make_all v357