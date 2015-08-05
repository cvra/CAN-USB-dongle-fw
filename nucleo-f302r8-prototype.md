# ST Nucleo STM32F302R8 prototype modifications
## CAN
CAN_RX: PB8
CAN_TX: PB9

## USB
USB_DM: PA11
USB_DP: PA12

Add a 1.5K Pullup to USB_DP (see AN2606 "STM32 microcontroller system memory boot mode")

For booting into DFU connect BOOT0 pin to VCC with a jumper.

## Clock source
For USB to work an external clock source is required.
Use MCO clock output from ST-link:
    - SB54, SB55 OFF
    - R35,R37 removed
    - SB16, SB50 ON

Alternatively, mount the cristal (see Nucleo User Manual for modifications).
