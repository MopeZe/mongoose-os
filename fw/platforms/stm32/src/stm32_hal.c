/*
 * Copyright (c) 2014-2018 Cesanta Software Limited
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/time.h>

#include <stm32_sdk_hal.h>
//#include "stm32f7xx_hal_iwdg.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "common/cs_dbg.h"

#include "mongoose.h"

#include "mgos_hal.h"
#include "mgos_sys_config.h"
#include "mgos_mongoose.h"
#include "mgos_timers.h"
#include "mgos_utils.h"

#include "stm32_uart.h"

void mgos_dev_system_restart(void) {
  HAL_NVIC_SystemReset();
}

void device_get_mac_address(uint8_t mac[6]) {
  static uint8_t s_dev_mac[6] = {0};
  if (s_dev_mac[0] != 0) {
    memcpy(mac, s_dev_mac, 6);
    return;
  }
  /*
   * Construct MAC address by using a Locally Administered Address OUI 12:34:...
   * and a unique suffix obtained from hashing the device's UID.
   */
  uint32_t uid[3] = {
      READ_REG(*((uint32_t *) UID_BASE)),
      READ_REG(*((uint32_t *) UID_BASE + 4)),
      READ_REG(*((uint32_t *) UID_BASE + 8)),
  };
  uint8_t digest[20];
  const uint8_t *msgs[1] = {(const uint8_t *) &uid[0]};
  size_t lens[1] = {sizeof(uid)};
  mg_hash_sha1_v(1, msgs, lens, digest);
  memcpy(&s_dev_mac[2], digest, 4);
  s_dev_mac[1] = 0x34;
  s_dev_mac[0] = 0x12;
  memcpy(mac, s_dev_mac, 6);
}

void mgos_msleep(uint32_t msecs) {
  mgos_usleep(msecs * 1000);
}

void HAL_Delay(__IO uint32_t ms) __attribute__((alias("mgos_msleep")));

static void __attribute__((naked)) delay_cycles(unsigned long ulCount) {
  __asm(
      "    subs    r0, #1\n"
      "    bne     delay_cycles\n"
      "    bx      lr");
}

void mgos_usleep(uint32_t usecs) {
  int ticks = usecs / (1000000 / configTICK_RATE_HZ);
  int remainder = usecs % (1000000 / configTICK_RATE_HZ);
  if (ticks > 0) vTaskDelay(ticks);
  if (remainder > 0) delay_cycles(remainder * (SystemCoreClock / 1000000));
}

/* Note: PLL must be enabled for RNG to work */
int mg_ssl_if_mbed_random(void *ctx, unsigned char *buf, size_t len) {
  RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
  RNG->CR = RNG_CR_RNGEN;
  int i = 0;
  do {
    if (RNG->SR & RNG_SR_DRDY) {
      uint32_t rnd = RNG->DR;
      size_t l = MIN(len - i, sizeof(rnd));
      memcpy(buf + i, &rnd, l);
      i += l;
    }
  } while (i < len);
  (void) ctx;
  return 0;
}

uint32_t HAL_GetTick(void) {
  /* Overflow? Meh. HAL doesn't seem to care. */
  return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/* LwIP time function, returns timestamp in milliseconds. */
u32_t sys_now(void) {
  return HAL_GetTick();
}

#define IWDG_1_SECOND 128
IWDG_HandleTypeDef hiwdg = {
    .Instance = IWDG,
    .Init =
        {
         .Prescaler = IWDG_PRESCALER_256,
         .Reload = 5 * IWDG_1_SECOND,
#ifdef STM32F7
         .Window = IWDG_WINDOW_DISABLE,
#endif
        },
};

void mgos_wdt_enable(void) {
  HAL_IWDG_Init(&hiwdg);
}

void mgos_wdt_feed(void) {
  HAL_IWDG_Refresh(&hiwdg);
}

void mgos_wdt_set_timeout(int secs) {
  uint32_t new_reload = (secs * IWDG_1_SECOND);
  if (!IS_IWDG_RELOAD(new_reload)) {
    LOG(LL_ERROR, ("Invalid WDT reload value %lu", new_reload));
    return;
  }
  hiwdg.Init.Reload = new_reload;
  HAL_IWDG_Init(&hiwdg);
}

void mgos_wdt_disable(void) {
  static bool printed = false;
  if (!printed) {
    printed = true;
    LOG(LL_ERROR, ("Once enabled, WDT cannot be disabled!"));
  }
}

void mgos_bitbang_write_bits_js(void) {
  /* TODO */
}

uint32_t mgos_get_cpu_freq(void) {
  return SystemCoreClock;
}
