###############################################################################
#
# Makefile - Makefile for mcu_test umqtt demo program
#
# Copyright (c) 2017, Joseph Kroesche (tronics.kroesche.io)
# All rights reserved.
#
# This software is released under the FreeBSD license, found in the
# accompanying file LICENSE.txt and at the following URL:
#      http://www.freebsd.org/copyright/freebsd-license.html
#
# This software is provided as-is and without warranty.
#
###############################################################################

# By default this application is built using TivaWare.  If you want to use
# StellarisWare instead, then uncomment the following line
#
USE_STELLARISWARE=1

# path to StellarisWare or TivaWare installed on the system
# You can also set this in the environment which will override this value
ifeq (${USE_STELLARISWARE}, 1)
TIVAWARE_ROOT?=/Users/joe/Documents/Projects/tronics/ti/StellarisWare-LM3S-10636
else
TIVAWARE_ROOT?=/Users/joe/Documents/Projects/tronics/ti/TivaWare_C_Series_2.0.1.11577
endif

# name of the program
APPNAME=mcu_test

# list of C source files
SRCS=mcu_test.c
SRCS+=net_client.c
SRCS+=swtimer.c
SRCS+=../umqtt/umqtt.c
SRCS+=startup_gcc.c
SRCS+=${TIVAWARE_ROOT}/utils/lwiplib.c
SRCS+=${TIVAWARE_ROOT}/utils/ustdlib.c
SRCS+=${TIVAWARE_ROOT}/utils/uartstdio.c

# make search path for source files
VPATH=
VPATH+=${TIVAWARE_ROOT}/utils
VPATH+=../umqtt

# list of all the include paths
INCLUDES=-I.
INCLUDES+=-I${TIVAWARE_ROOT}
INCLUDES+=-I..
INCLUDES+=-I${TIVAWARE_ROOT}/third_party/lwip-1.3.2/apps
INCLUDES+=-I${TIVAWARE_ROOT}/third_party/lwip-1.3.2/ports/stellaris/include
INCLUDES+=-I${TIVAWARE_ROOT}/third_party/lwip-1.3.2/src/include
INCLUDES+=-I${TIVAWARE_ROOT}/third_party/lwip-1.3.2/src/include/ipv4

# list of libraries to include in the link
ifeq (${USE_STELLARISWARE}, 1)
LIBS=${TIVAWARE_ROOT}/driverlib/gcc-${CORTEXM}/libdriver-${CORTEXM}.a
else
LIBS=${TIVAWARE_ROOT}/driverlib/gcc/libdriver.a
endif

# defines the part number
ifeq (${USE_STELLARISWARE}, 1)
PART=LM3S6965 # this line is for StellarisWare
else
PART=TM4C123GH6PM # this line is for TivaWare
endif

# defines the part class.  this is used for ROM calls.  this can be left
# blank if ROM not used
ifeq (${USE_STELLARISWARE}, 1)
PART_CLASS=-DTARGET_IS_BLIZZARD_RA2 # this line is for StellarisWare
else
PART_CLASS=-DTARGET_IS_BLIZZARD_RA3 # this line is for TivaWare
endif

# the type of CORTEX-M, either cm3 or cm4f
CORTEXM=cm3

# the name of the linker script
LDSCRIPT=lm3s6965.ld

# name of entry point
ENTRY=ResetISR

# name of the output directory for objs and binaries
OUT=gcc/

# name of map file
MAPFILE=${OUT}${APPNAME}.map

#
# Set this to the path of the toolchain.  If it is already on your path
# you may be able to leave this blank.  However, if using xcode to invoke
# the makefile, it is hard to make xcode find your compiler even when on the
# path so I defined it here.
# This can also be set in the environment.
#
TOOLPATH?=/usr/local/gcc-arm-none-eabi-4_7-2013q3/bin/

# prefix name for tool chain
TOOLCHAIN=arm-none-eabi

# part definition macro needed by some TivaWare/StellarisWare files
PARTMACRO=PART_${PART}

# define the tools program names
CC=${TOOLPATH}${TOOLCHAIN}-gcc
AR=${TOOLPATH}${TOOLCHAIN}-ar
LD=${TOOLPATH}${TOOLCHAIN}-ld
OBJCOPY=${TOOLPATH}${TOOLCHAIN}-objcopy
CC=${TOOLPATH}${TOOLCHAIN}-gcc

# set up processor and floating point options per the core type
ifeq (${CORTEXM}, cm4f)
CPU=-mcpu=cortex-m4
FPU=-mfpu=fpv4-sp-d16 -mfloat-abi=softfp
else
CPU=-mcpu=cortex-m3
FPU=
endif

# stellaris standard C flags
CFLAGS=-g -mthumb ${CPU} ${FPU} -Os -ffunction-sections -fdata-sections -MD -std=c99 -Wall -pedantic

# Set a macro that can be used by the source code to detect if
# StellarisWare is being used
ifeq (${USE_STELLARISWARE}, 1)
CFLAGS+=-DSTELLARISWARE
endif

# stellaris standard linker flags
LDFLAG=--gc-sections

# some StellarisWare files need the part macro defined and expect gcc to be defined
CFLAGS+=-D${PARTMACRO}
CFLAGS+=-Dgcc
CFLAGS+=${PART_CLASS}

#CFLAGS+=-DDEBUG

# get gcc library locations
LIBM=${shell ${CC} ${CFLAGS} -print-file-name=libm.a}
LIBC=${shell ${CC} ${CFLAGS} -print-file-name=libc.a}
LIBGCC=${shell ${CC} ${CFLAGS} -print-libgcc-file-name}

# derive the object file list from the C sources
OBJS=$(addprefix ${OUT}, $(notdir $(SRCS:%.c=%.o)))

# rule for compiling
gcc/%.o: %.c
	${CC} ${CFLAGS} ${INCLUDES} -o ${@} -c ${<}

# rule for linking
%.axf:
	${LD} -T ${LDSCRIPT} --entry ${ENTRY} ${LDFLAGS} -Map ${MAPFILE} -o ${@} ${OBJS} ${LIBS} '${LIBM}' '${LIBC}' '${LIBGCC}'
	${OBJCOPY} -O binary ${@} ${@:.axf=.bin}

# rule to build all
all: ${OUT} ${OUT}${APPNAME}.axf

#rule for cleaning
clean:
	rm -rf ${OUT}
	
# rule to make the output directory
${OUT}:
	mkdir ${OUT}

# dependency rule for building app
${OUT}${APPNAME}.axf: ${OBJS} ${LIBS} ${LDSCRIPT} Makefile ${OUT}

# include compiler generated dependencies
ifneq ($(MAKECMDGOALS), clean)
-include $(SRCS:%.c=$(OUT)%.d)
endif
