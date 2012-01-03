#
# STM32F4 USB bootloader build rules.
#

BINARY		 = px4fmu_bl.elf

LIBOPENCM3	?= /usr/local/arm-none-eabi

CC		 = arm-none-eabi-gcc

SRCS		 = bl.c cdcacm.c

OBJS		 = $(patsubst %.c,%.o,$(SRCS))
DEPS		 = $(patsubst %.o,%.d,$(OBJS))
EXTRA_DEPS	 = $(MAKEFILE_LIST)

CFLAGS		 = -mthumb -mcpu=cortex-m4 \
		   -g \
		   -Wall \
		   -MD \
		   -fno-builtin \
		   -I$(LIBOPENCM3)/include \
		   -DSTM32F4 \
		   -DOSC_FREQ=24 \
		   -DAPP_LOAD_ADDRESS=0x08004000 \
		   -DAPP_SIZE_MAX=0xfc000

LDFLAGS		 = -mthumb -mcpu=cortex-m4 \
		   -nostartfiles \
		   -lnosys \
		   -Tstm32f4.ld \
		   -L$(LIBOPENCM3)/lib/stm32/f4 \
		   -lopencm3_stm32f4

all:		$(BINARY)

$(BINARY):	$(OBJS) $(EXTRA_DEPS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o:		%.c $(EXTRA_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(OBJS) $(DEPS) $(BINARY)

-include $(DEPS)
