# GR-LYCHEE_Switching_BlockDevice
**Please see [here](README_JPN.md) for Japanese version.**  
This is a sample program that works on GR-LYCHEE.  

## Overview
It is a sample program that can read and write various BlockDevice from PC on Filesystem.

## Requirements
* [GR-LYCHEE](https://os.mbed.com/platforms/Renesas-GR-LYCHEE/) (Required)
* [Winbond Flash Memory Sample Shield](http://www.winbond.com/hq/about-winbond/news-and-events/events/product-promotion/promotion00020.html) (Optional)
* SD card (Optional)
* USB memory (Optional)
* Audio speaker (Optional)

You can use Serial NOR of ``W25M161AVEIT`` and ``W74M12FVZPIQ`` on ``Winbond Flash Memory Sample Shield``. Switch devices using Jumper on Shield. Please refer to User Guide for Jumper setting.  
When not using Winbond Flash Memory Sample Shield, set `0` to the following macro of` main.cpp`.
```cpp
#define USE_SAMPLE_SHIELD (1) // [Winbond Flash Memory Sample Shield] 0: not use, 1: use
```

## How to use
By connecting the GR-LYCHEE and the PC with a USB cable, you can refer inside the device as a mass storage device. (The format is required when connecting for the first time.)  
If you are connecting an audio speaker, writing a WAV file (.wav) from the PC to the storage will play that song. Pressing `USER_BUTTON0` plays the next song.  

Pressing `USER_BUTTON1` BlockDevice switches in the following order.   

| BlockDevice | Description |
|: ------------ |: ------------ |
| SPIF | The Serial NOR on "Winbond Flash Memory Sample Shield" is used as storage. |
| FlashIap | Serial Flash on GR-LYCHEE. The first half 1 MB is used as a program, the latter half 7 MB is used as storage. |
| Heap | 1 MB of heap memory is used as storage. |
| SD | SD is used as storage. |
| USB | Uses USB memory as storage. |
