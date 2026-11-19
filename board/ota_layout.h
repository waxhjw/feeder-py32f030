#ifndef FEEDER_OTA_LAYOUT_H
#define FEEDER_OTA_LAYOUT_H
#include <stdint.h>
/* 仅适用于本板64KB PY32F030F18P6TU；Boot与应用采用同一布局。 */
#define OTA_FLASH_BASE 0x08000000UL
#define OTA_APP_BASE   0x08002000UL
#define OTA_APP_LIMIT  0x08008000UL
#define OTA_STAGE_BASE 0x08008000UL
#define OTA_STAGE_SIZE 0x00006000UL
#define OTA_ACTIVE     0x0800E000UL
#define OTA_PENDING    0x0800E080UL
#define OTA_PAGE       128U
#define OTA_HEADER     128U
#define OTA_MAX_IMAGE  (OTA_STAGE_SIZE-OTA_HEADER)
#define OTA_MAGIC      0x3141544FUL
#define OTA_BOARD      0x46303330UL
typedef struct {
    uint32_t magic, format, board, version, size, crc, load, header_crc;
    uint8_t reserved[96];
} OtaHeader;
_Static_assert(sizeof(OtaHeader)==OTA_HEADER,"OTA header ABI");
uint32_t ota_crc32(const void *data,uint32_t length);
int ota_header_valid(const OtaHeader *h);
int ota_image_valid(const OtaHeader *h,const uint8_t *image);
/* 由平台驱动或离线Flash模型实现：整页擦写、读回，禁止选项字节/全片擦除。 */
const uint8_t *ota_read(uint32_t address);
int ota_write_page(uint32_t address,const uint32_t *data);
int ota_erase_page(uint32_t address);
void ota_watchdog(void);
int ota_boot_install(void);
#endif
