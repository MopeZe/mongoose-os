DEVICE = stm32f746zg
# No XTAL on this board
HSE_VALUE = 0
STM32_CFLAGS += -DBSP_NUCLEO_F746ZG
#                          B0,           B7,           B14
STM32_CFLAGS += -DLED1_PIN=16 -DLED2_PIN=23 -DLED3_PIN=30
MGOS_DEBUG_UART ?= 3
STM32_CONF_SCHEMA += $(MGOS_PLATFORM_PATH)/mk/boards/NUCLEO-F746ZG_sys_config.yaml
