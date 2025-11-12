#!/bin/bash
# Rebuild and update the precompiled binaries.
rm -f *.hex *.bin *.uf2
cd ..
pio run -e microbit
cp .pio/build/microbit/firmware.hex precompiled/vm_microbitV1.hex
pio run -e microbitV2-ble
cp .pio/build/microbitV2-ble/firmware.hex precompiled/vm_microbitV2.hex
pio run -e calliope
cp .pio/build/calliope/firmware.hex precompiled/vm_calliope.hex
pio run -e calliopeV3-ble
cp .pio/build/calliopeV3-ble/firmware.hex precompiled/vm_calliopeV3-ble.hex
pio run -e nodemcu
cp .pio/build/nodemcu/firmware.bin precompiled/vm_nodemcu.bin
pio run -e ed1
cp .pio/build/ed1/firmware.bin precompiled/vm_citilab-ed1.bin
pio run -e cocube
cp .pio/build/cocube/firmware.bin precompiled/vm_cocube.bin

pio run -e esp32
cp .pio/build/esp32/firmware.bin precompiled/vm_esp32.bin
pio run -e cpx
python precompiled/uf2conv.py -c .pio/build/cpx/firmware.bin -o precompiled/vm_circuitplay.uf2
pio run -e cplay52
python precompiled/uf2conv.py -c .pio/build/cplay52/firmware.hex -f 0xADA52840 -o precompiled/vm_cplay52.uf2
pio run -e pico-w
cp .pio/build/pico-w/firmware.uf2 precompiled/vm_pico_w.uf2
pio run -e wukong2040
cp .pio/build/wukong2040/firmware.uf2 precompiled/vm_wukong2040.uf2
pio run -e databot
cp .pio/build/databot/firmware.bin precompiled/vm_databot.bin
pio run -e steam-maker
cp .pio/build/steam-maker/firmware.bin precompiled/vm_micro_steamakers.bin
pio run -e kids-bits
cp .pio/build/kids-bits/firmware.bin precompiled/vm_kids_bits.bin
pio run -e foxbit
cp .pio/build/foxbit/firmware.bin precompiled/vm_foxbit.bin
pio run -e makerportV3
python precompiled/uf2conv.py -c .pio/build/makerportV3/firmware.bin -o precompiled/vm_makerport.uf2

# Copy Linux VMs
cp linux+pi/vm_* precompiled/

# Create micro:bit Universal Hex File
# Make sure we have the proper dep(s) installed
cd precompiled
npm install
node buildUniversalHex.js
rm vm_microbitV1.hex vm_microbitV2.hex
rm vm_calliope.hex vm_calliopeV3-ble.hex

