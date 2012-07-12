#
# Common Makefile for the PX4 bootloaders
#

#
# Paths to common dependencies
#
export LIBOPENCM3	?= ../libopencm3

#
# Tools
#
export CC	 	 = arm-none-eabi-gcc

#
# Common configuration
#
export FLAGS		 = -Os \
			   -g \
			   -Wall \
			   -fno-builtin \
			   -I$(LIBOPENCM3)/include \
			   -ffunction-sections \
			   -nostartfiles \
			   -lnosys \
	   		   -Wl,-gc-sections

export COMMON_SRCS	 = bl.c

#
# Bootloaders to build
#
TARGETS			 = px4fmu_bl px4flow_bl stm32f4discovery_bl px4io_bl

# px4io_bl px4flow_bl

all:	$(TARGETS)

clean:
	rm -f *.elf

#
# Specific bootloader targets.
#
# Pick a Makefile from Makefile.f1, Makefile.f4
# Pick an interface supported by the Makefile (USB, UART, I2C)
# Specify the board type.
#

px4fmu_bl:
	make -f Makefile.f4 TARGET=fmu INTERFACE=USB BOARD=FMU

stm32f4discovery_bl:
	make -f Makefile.f4 TARGET=discovery INTERFACE=USB BOARD=DISCOVERY

px4flow_bl:
	make -f Makefile.f4 TARGET=flow INTERFACE=USB BOARD=FLOW

# PX4IO waits in the bootloader for FMU to check the firmware version before startup.
# Delay here is a bit over 48 days.  FMU should boot faster than that.
#
px4io_bl:
	make -f Makefile.f1 TARGET=io INTERFACE=USART BOARD=IO
