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
TARGETS			 = px4fmu_bl
# px4io_bl px4flow_bl

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

px4fmu_bl:
	make -f Makefile.f4 TARGET=fmu INTERFACE=USB BOARD=FMU

stm32f4discovery_bl:
	make -f Makefile.f4 TARGET=fmu INTERFACE=USB BOARD=DISCOVERY

px4flow_bl:
	make -f Makefile.f4 TARGET=flow INTERFACE=USB BOARD=FLOW

px4io_bl:
	make -f Makefile.f1 TARGET=io INTERFACE=I2C BOARD=IO
