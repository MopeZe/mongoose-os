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

#include <stdio.h>

#include <stm32_sdk_hal.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "lwip/tcpip.h"

#include "common/cs_dbg.h"

#include "mgos_app.h"
#include "mgos_hal_freertos_internal.h"
#include "mgos_init_internal.h"
#include "mgos_debug.h"
#include "mgos_debug_internal.h"
#include "mgos_gpio.h"
#include "mgos_mongoose_internal.h"
#include "mgos_sys_config.h"
#include "mgos_system.h"
#include "mgos_uart_internal.h"

#include "stm32_fs.h"
#include "stm32_hal.h"
#include "stm32_uart.h"

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority) {
  /* Override the HAL function but do nothing, FreeRTOS will take care of it. */
  (void) TickPriority;
  return 0;
}

static void tcpip_init_done(void *arg) {
  *((bool *) arg) = true;
}

static void set_nocache(void) {
  extern uint8_t __nocache_start__, __nocache_end__; /* Linker symbols */

  int num_regions = (MPU->TYPE & MPU_TYPE_DREGION_Msk) >> MPU_TYPE_DREGION_Pos;
  int prot_size = (&__nocache_end__ - &__nocache_start__);
  if (prot_size == 0) return;
  if (num_regions == 0) {
    LOG(LL_ERROR, ("Memory protection is requested but not available"));
    return;
  }
  if (prot_size > NOCACHE_SIZE) {
    LOG(LL_ERROR, ("Max size of protected region is %d", NOCACHE_SIZE));
    return;
  }
  /* Protected regions must be size-aligned. */
  if ((((uintptr_t) &__nocache_start__) & (NOCACHE_SIZE - 1)) != 0) {
    LOG(LL_ERROR, ("Protected region must be %d-aligned", NOCACHE_SIZE));
    return;
  }

  MPU_Region_InitTypeDef MPU_InitStruct;

  HAL_MPU_Disable();

  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = (uintptr_t) &__nocache_start__;
#if NOCACHE_SIZE != 0x400
#error NOCACHE_SIZE must be 1K
#endif
  MPU_InitStruct.Size = MPU_REGION_SIZE_1KB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

  LOG(LL_DEBUG,
      ("Marked [%p, %p) as no-cache", &__nocache_start__, &__nocache_end__));
}

enum mgos_init_result mgos_hal_freertos_pre_init() {
  set_nocache();
  volatile bool lwip_inited = false;
  tcpip_init(tcpip_init_done, (void *) &lwip_inited);
  while (!lwip_inited)
    ;
  return MGOS_INIT_OK;
}

extern void mgos_nsleep100_cal();

void SystemCoreClockUpdate(void) {
  uint32_t presc = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> 4)];
  SystemCoreClock = HAL_RCC_GetSysClockFreq() >> presc;
  mgos_nsleep100_cal();
}

extern void stm32_clock_config(void);

int main() {
  stm32_clock_config();

  SystemCoreClockUpdate();

  mgos_hal_freertos_run_mgos_task(true /* start_scheduler */);
  /* not reached */
  abort();
}

void assert_failed(uint8_t *file, uint32_t line) {
  fprintf(stderr, "assert_failed @ %s:%d\r\n", file, (int) line);
  abort();
}
