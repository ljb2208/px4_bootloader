#
# STM32F4 USB bootloader build rules.
#

BINARY		 = px4fmu_bl.elf

LIBOPENCM3	?= /usr/local/arm-none-eabi

PX4FMU	= 1
STM32F4DISCOVERY	= 2
PX4FLOW	= 3

PX4_BOARD_TYPE	?= PX4FLOW
#PX4_BOARD_TYPE	?= PX4FMU
#PX4_BOARD_TYPE	?= STM32F4DISCOVERY

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
		   -DBOARD=$(PX4_BOARD_TYPE) \
		   -ffunction-sections

LDFLAGS		 = -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
		   -nostartfiles \
		   -lnosys \
		   -Tstm32f4.ld \
		   -L$(LIBOPENCM3)/lib \
		   -lopencm3_stm32f4 \
		   -Wl,-gc-sections

all:		$(BINARY)

$(BINARY):	$(OBJS) $(EXTRA_DEPS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o:		%.c $(EXTRA_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(OBJS) $(DEPS) $(BINARY)

-include $(DEPS)
