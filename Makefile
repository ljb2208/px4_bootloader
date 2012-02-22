#
# STM32F4 USB bootloader build rules.
#

BINARY		 = px4fmu_bl.elf

LIBOPENCM3	?= ../libopencm3
OPENOCD		?= ../../sat/bin/openocd

JTAGCONFIG ?= interface/olimex-jtag-tiny.cfg
#JTAGCONFIG ?= interface/jtagkey-tiny.cfg

PX4FMU	= 1
STM32F4DISCOVERY	= 2
PX4FLOW	= 3

#PX4_BOARD_TYPE	?= PX4FLOW
PX4_BOARD_TYPE	?= PX4FMU
#PX4_BOARD_TYPE	?= STM32F4DISCOVERY

# 5 seconds / 5000 ms default delay
PX4_BOOTLOADER_DELAY	?= 5000

CC		 = arm-none-eabi-gcc

SRCS		 = bl.c cdcacm.c

OBJS		 = $(patsubst %.c,%.o,$(SRCS))
DEPS		 = $(patsubst %.o,%.d,$(OBJS))
EXTRA_DEPS	 = $(MAKEFILE_LIST)

CFLAGS		 = -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
		   -Os \
		   -Wall \
		   -MD \
		   -fno-builtin \
		   -I$(LIBOPENCM3)/include \
		   -DSTM32F4 \
		   -DOSC_FREQ=24 \
		   -DAPP_LOAD_ADDRESS=0x08004000 \
		   -DAPP_SIZE_MAX=0xfc000 \
		   -DBOOTLOADER_DELAY=$(PX4_BOOTLOADER_DELAY) \
		   -DBOARD=$(PX4_BOARD_TYPE) \
		   -ffunction-sections

LDFLAGS		 = -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
		   -nostartfiles \
		   -lnosys \
		   -Tstm32f4.ld \
		   -L$(LIBOPENCM3)/lib/stm32/f4/ \
		   -lopencm3_stm32f4 \
		   -Wl,-gc-sections

all:		$(BINARY)

$(BINARY):	$(OBJS) $(EXTRA_DEPS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o:		%.c $(EXTRA_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(OBJS) $(DEPS) $(BINARY)

#upload: all flash flash-bootloader
upload: all flash-bootloader

flash-bootloader:
	$(OPENOCD) --search ../px4_bootloader -f $(JTAGCONFIG) -f stm32f4x.cfg -c init -c "reset halt" -c "flash write_image erase px4fmu_bl.elf" -c "reset run" -c shutdown





-include $(DEPS)
