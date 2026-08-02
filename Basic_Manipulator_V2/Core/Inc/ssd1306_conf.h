#ifndef SSD1306_CONF_H
#define SSD1306_CONF_H

#include "stm32f1xx_hal.h"
#include <stdlib.h>
#include <string.h>

// ===== ВЫБЕРИТЕ ИНТЕРФЕЙС =====
#define SSD1306_USE_I2C   // <--- ЭТО ОБЯЗАТЕЛЬНО!

// ===== НАСТРОЙКИ =====
#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64
#define SSD1306_I2C_ADDR 0x78
#define SSD1306_BUFFER_SIZE (SSD1306_WIDTH * SSD1306_HEIGHT / 8)

extern I2C_HandleTypeDef hi2c1;

#endif /* SSD1306_CONF_H */
