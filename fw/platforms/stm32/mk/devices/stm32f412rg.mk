FAMILY = stm32f4
SRAM_SIZE = 262144
FLASH_SIZE = 1048576
STM32_CFLAGS += -DSTM32F412Rx -DMGOS_NUM_GPIO=114
MGOS_SRCS += stm32f412.c
