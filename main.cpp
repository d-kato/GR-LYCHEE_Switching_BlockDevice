#include "mbed.h"
#include "EasyPlayback.h"
#include "EasyDec_WavCnv2ch.h"

#include "USBMSD.h"
#include "ObservingBlockDevice.h"
#include "SPIFBlockDevice.h"
#include "FlashIAPBlockDevice_2.h"
#include "mbed_drv_cfg.h"
#include "HeapBlockDevice.h"
#include "SDBlockDevice_GRBoard.h"
#include "USBHostMSD.h"

#define FILE_NAME_LEN          (64)
#define USE_SAMPLE_SHIELD      (1)  // [Winbond Flash Memory Sample Shield]  0:not use,  1:use

typedef enum {
#if USE_SAMPLE_SHIELD
    BD_SPI,
#endif
    BD_FLASHIAP,
    BD_HEAP,
    BD_SD,
    BD_USB,
    BD_NUM
} bd_type_t;

#if USE_SAMPLE_SHIELD
static SPIFBlockDevice spi_bd(D11, D12, D13, D10, 32000000);
static DigitalOut hold(D8);
#endif
static FlashIAPBlockDevice_2 flashiap_bd(FLASH_BASE + 0x100000, FLASH_SIZE - 0x100000);
static HeapBlockDevice heap_bd(0x100000);
static SDBlockDevice_GRBoard sd_bd;
static USBHostMSD usb_bd;

static BlockDevice * base_bd[BD_NUM] = {
#if USE_SAMPLE_SHIELD
    &spi_bd,         // BD_SPI
#endif
    &flashiap_bd,    // BD_FLASHIAP
    &heap_bd,        // BD_HEAP
    &sd_bd,          // BD_SD
    &usb_bd          // BD_USB
};

static const char * base_bd_str[BD_NUM] = {
#if USE_SAMPLE_SHIELD
    "SPIF",          // BD_SPI
#endif
    "FlashIAP",      // BD_FLASHIAP
    "Heap",          // BD_HEAP
    "SD",            // BD_SD
    "USB"            // BD_USB
};

static EasyPlayback AudioPlayer;
static InterruptIn skip_btn(USER_BUTTON0);
static InterruptIn storage_btn(USER_BUTTON1);
static int tmp_bd_index = 0;
static int bd_index = -1;
static bool storage_change_flg = false;
static Timer storage_change_timer;
static FATFileSystem * p_fs = NULL;
static ObservingBlockDevice * p_observing_bd = NULL;
static USBMSD * p_usb = NULL;
static Thread msdTask(osPriorityAboveNormal);
static Semaphore usb_sem(0);
static bool heap_bd_format = false;

static void storage_change(BlockDevice * p_bd) {
    storage_change_flg = true;
    storage_change_timer.reset();
    storage_change_timer.start();
    AudioPlayer.skip();
}

static void wait_storage_change(void) {
    while (storage_change_timer.read_ms() < 1000) {
        ThisThread::sleep_for(100);
    }
    storage_change_timer.stop();
    storage_change_timer.reset();
    storage_change_flg = false;
}

static void usb_callback_func(void) {
    usb_sem.release();
}

static void msd_task(void) {
    while (true) {
        usb_sem.wait();
        if (p_usb != NULL) {
            p_usb->process();
        }
    }
}

static void chk_bd_change(void) {
    if (p_fs != NULL) {
        p_fs->unmount();
    }
    if (bd_index != tmp_bd_index) {
        while (true) {
            bd_index = tmp_bd_index;
            printf("\r\nconnecting %s\r\n", base_bd_str[bd_index]);
#if USE_SAMPLE_SHIELD
            if (bd_index == BD_SPI) {
                hold = 1;
                break;
            } else 
#endif
            if (bd_index == BD_HEAP) {
                if (!heap_bd_format) {
                    FATFileSystem::format(&heap_bd);
                    heap_bd_format = true;
                }
                break;
            } else if (bd_index == BD_SD) {
                while ((!sd_bd.connect()) && (bd_index == tmp_bd_index)) {
                    ThisThread::sleep_for(500);
                }
                if (sd_bd.connected()) {
                    break;
                }
            } else if (bd_index == BD_USB) {
                while ((!usb_bd.connect()) && (bd_index == tmp_bd_index)) {
                    ThisThread::sleep_for(500);
                }
                if (usb_bd.connected()) {
                    break;
                }
            } else {
                break;
            }
        }
        if (p_fs != NULL) {
            delete p_fs;
        }
        p_fs = new FATFileSystem(base_bd_str[bd_index]);
        if (p_usb != NULL) {
            USBMSD * wk = p_usb;
            p_usb = NULL;
            delete wk;
        }
        if (p_observing_bd != NULL) {
            delete p_observing_bd;
            p_observing_bd = NULL;
        }
        p_observing_bd = new ObservingBlockDevice(base_bd[bd_index]);
        p_observing_bd->attach(&storage_change);
        p_usb = new USBMSD(p_observing_bd);
        p_usb->attach(&usb_callback_func);
        ThisThread::sleep_for(500);
    }
    if (p_fs != NULL) {
        p_fs->mount(p_observing_bd);
    }
}

static void storage_btn_fall(void) {
    if ((tmp_bd_index + 1) < BD_NUM) {
        tmp_bd_index++;
    } else {
        tmp_bd_index = 0;
    }
    storage_change(NULL);
    AudioPlayer.skip();
}

static void skip_btn_fall(void) {
    AudioPlayer.skip();
}

int main() {
    DIR  * d;
    struct dirent * p;
    char file_path[10 + FILE_NAME_LEN];
    int file_num;

    // decoder setting
    AudioPlayer.add_decoder<EasyDec_WavCnv2ch>(".wav");
    AudioPlayer.add_decoder<EasyDec_WavCnv2ch>(".WAV");

    // volume control
    AudioPlayer.outputVolume(0.5);  // Volume control (min:0.0 max:1.0)

    // button setting
    skip_btn.fall(&skip_btn_fall);
    storage_btn.fall(&storage_btn_fall);

    msdTask.start(&msd_task);
    storage_change(NULL);

    while (true) {
        if (storage_change_flg) {
            wait_storage_change();
            chk_bd_change();
        }

        // file search
        file_num = 0;
        sprintf(file_path, "/%s/", base_bd_str[bd_index]);
        d = opendir(file_path);
        if (d != NULL) {
            while ((p = readdir(d)) != NULL) {
                size_t len = strlen(p->d_name);
                if (len < FILE_NAME_LEN) {
                    // make file path
                    sprintf(file_path, "/%s/%s", base_bd_str[bd_index], p->d_name);
                    printf("%s\r\n", file_path);

                    // playback
                    if (AudioPlayer.play(file_path)) {
                        file_num++;
                    }
                    if (storage_change_flg) {
                        break;
                    }
                }
            }
            closedir(d);
        }
        if (file_num == 0) {
            printf("No files\r\n");
            while (!storage_change_flg) {
                ThisThread::sleep_for(100);
            }
        }
    }
}
