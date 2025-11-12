#!/bin/bash
# Rebuild and update the precompiled binaries.
rm -f *.hex *.bin *.uf2
cd ..

pio run -e clue
python precompiled/uf2conv.py -c -f 0xada52840 .pio/build/clue/firmware.hex -o extraVMs/vm_clue.uf2
pio run -e pico-ed
cp .pio/build/pico-ed/firmware.uf2 extraVMs/vm_pico_ed.uf2
pio run -e m5stack
cp .pio/build/m5stack/firmware.bin extraVMs/vm_m5stack.bin

pio run -e xiao-samd21
python precompiled/uf2conv.py -c .pio/build/xiao-samd21/firmware.bin -o extraVMs/vm_xiao_samd21.uf2
pio run -e xiao-nrf52840
python precompiled/uf2conv.py -c .pio/build/xiao-nrf52840/firmware.hex -f 0xADA52840 -o extraVMs/vm_xiao_nrf52840.uf2
pio run -e xiao-rp2040
cp .pio/build/xiao-rp2040/firmware.uf2 extraVMs/vm_xiao_rp2040.uf2
pio run -e xiao-rp2350
cp .pio/build/xiao-rp2350/firmware.uf2 extraVMs/vm_xiao_rp2350.uf2

pio run -e xiao-esp32-c3
cp .pio/build/xiao-esp32-c3/firmware.bin extraVMs/vm_xiao_esp32_c3.bin
pio run -e xiao-esp32-s3
cp .pio/build/xiao-esp32-s3/firmware.bin extraVMs/vm_xiao_esp32_s3.bin

pio run -e itsybitsy
python precompiled/uf2conv.py -c .pio/build/itsybitsy/firmware.bin -o extraVMs/vm_itsybitsy.uf2
pio run -e metroM0
python precompiled/uf2conv.py -c .pio/build/metroM0/firmware.bin -o extraVMs/vm_metroM0.uf2
pio run -e mbits
cp .pio/build/mbits/firmware.bin extraVMs/vm_mbits.bin
pio run -e pico-xrp
cp .pio/build/pico-xrp/firmware.uf2 extraVMs/vm_pico_xrp.uf2
pio run -e gizmo-mechatronics
cp .pio/build/gizmo-mechatronics/firmware.uf2 extraVMs/vm_gizmo_mechatronics.uf2
pio run -e wukong2040-w
cp .pio/build/wukong2040-w/firmware.uf2 extraVMs/vm_wukong2040_w.uf2

pio run -e m5atom
cp .pio/build/m5atom/firmware.bin extraVMs/vm_m5atom.bin
pio run -e m5atom-lite
cp .pio/build/m5atom-lite/firmware.bin extraVMs/vm_m5atom_lite.bin
pio run -e m5atom-s3-lite
cp .pio/build/m5atom-s3-lite/firmware.bin extraVMs/vm_m5atom_s3_lite.bin
pio run -e m5atom-s3-tft
cp .pio/build/m5atom-s3-tft/firmware.bin extraVMs/vm_m5atom_s3_tft.bin

pio run -e m5stick
cp .pio/build/m5stick/firmware.bin extraVMs/vm_m5stick.bin
pio run -e m5stick-plus
cp .pio/build/m5stick-plus/firmware.bin extraVMs/vm_m5stick_plus.bin

pio run -e m5core2
cp .pio/build/m5core2/firmware.bin extraVMs/vm_m5core2_1.0.bin

pio run -e esp32-s3
cp .pio/build/esp32-s3/firmware.bin extraVMs/vm_esp32-s3.bin
pio run -e esp32-c3-dev
cp .pio/build/esp32-c3-dev/firmware.bin extraVMs/vm_esp32-c3-dev.bin
pio run -e esp32-c3-qtpy
cp .pio/build/esp32-c3-qtpy/firmware.bin extraVMs/vm_esp32-c3-qtpy.bin
pio run -e esp32-c3-mini
cp .pio/build/esp32-c3-mini/firmware.bin extraVMs/vm_esp32-c3-mini.bin

pio run -e waveshare-s3-matrix
esptool.py --chip ESP32-S3 merge_bin -o extraVMs/vm_waveshare_s3_matrix_all.bin --flash_mode dio --flash_size 4MB 0 .pio/build/waveshare-s3-matrix/bootloader.bin 0x8000 esp32/partitionsMicroBlocks.bin 0xe000 esp32/boot_app0.bin 0x10000 .pio/build/waveshare-s3-matrix/firmware.bin
// the following runs on the server during the release process:
python3 -m esptool --chip ESP32-S3 merge_bin -o extraVMs/vm_waveshare_s3_matrix_all.bin --flash_mode dio --flash_size 4MB 0 .pio/build/waveshare-s3-matrix/bootloader.bin 0x8000 esp32/partitionsMicroBlocks.bin 0xe000 esp32/boot_app0.bin 0x10000 .pio/build/waveshare-s3-matrix/firmware.bin

pio run -e freenoveCamera
cp .pio/build/freenoveCamera/firmware.bin extraVMs/vm_freenoveCamera.bin

pio run -e rp2350
cp .pio/build/rp2350/firmware.uf2 extraVMs/vm_rp2350.uf2

pio run -e rp2350-w
cp .pio/build/rp2350-w/firmware.uf2 extraVMs/vm_rp2350_w.uf2
