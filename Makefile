PROJECT=bitbox-da14531-firmware

SDKPATH=/opt/DA145xx_SDK/6.0.22.1401/sdk

SDK_SOURCES=$(addprefix ${SDKPATH}/,\
	    platform/driver/dma/dma.c \
	    platform/driver/uart/uart.c \
	    platform/driver/gpio/gpio.c \
	    platform/driver/uart/uart_utils.c \
	    platform/driver/hw_otpc/hw_otpc_531.c \
	    platform/driver/syscntl/syscntl.c \
	    platform/utilities/otp_cs/otp_cs.c \
	    platform/arch/main/arch_system.c \
	    platform/arch/main/hardfault_handler.c \
	    platform/arch/main/nmi_handler.c \
	    platform/arch/boot/system_DA14531.c)

SDK_ASS_SOURCES=$(addprefix ${SDKPATH}/platform/arch/boot/GCC/,ivtable_DA14531.S startup_DA14531.S)

VPATH=src $(dir ${SDK_SOURCES}) $(dir ${SDK_ASS_SOURCES})

SOURCES=\
	$(addprefix src/,main.c ring_buffer.c uart_loopback_examples.c uart_receive_examples.c uart_send_examples.c user_periph_setup.c crc.c) \
	${SDK_SOURCES}

OBJECTS=$(notdir $(patsubst %.S,%.o,${SDK_ASS_SOURCES}) $(patsubst %.c,%.o,${SOURCES}))

DEFINES=-D__DA14531__ \
	-DCFG_UART_DMA_SUPPORT \
	-D__NON_BLE_EXAMPLE__

SDKINCLUDES=\
	    include \
	    platform/arch \
	    platform/arch/compiler \
	    platform/arch/main \
	    platform/arch/ll \
	    platform/core_modules/nvds/api \
	    platform/core_modules/common/api \
	    platform/core_modules/rwip/api \
	    platform/core_modules/rf/api \
	    platform/driver/adc \
	    platform/driver/dma \
	    platform/driver/gpio \
	    platform/driver/syscntl \
	    platform/driver/uart \
	    platform/driver/hw_otpc \
	    platform/utilities/otp_hdr \
	    platform/utilities/otp_cs \
	    platform/system_library/include \
	    platform/include \
	    platform/include/CMSIS/5.9.0/CMSIS/Core/Include

INCLUDE_DIRECTORIES=\
		    -Iinclude\
		    $(addprefix -I${SDKPATH}/,${SDKINCLUDES})

CPPFLAGS=-mthumb -mcpu=cortex-m0plus ${DEFINES} ${INCLUDE_DIRECTORIES}

CFLAGS=-Os -ffunction-sections -fdata-sections -Wall --specs=nano.specs --specs=nosys.specs -g3

LDLIBS=${SDKPATH}/platform/system_library/output/Keil_5/da14531.lib --specs=nano.specs --specs=nosys.specs

LDFLAGS=-mthumb -mcpu=cortex-m0plus -Wl,-T,firmware.lds -Wl,--gc-sections -Wl,-Map,output.map -L${SDKPATH}/common_project_files/misc

CC=arm-none-eabi-gcc

all: ${PROJECT}.o

${PROJECT}.o: ${PROJECT}.bin
	arm-none-eabi-objcopy -I binary -O elf32-littlearm --rename-section .data=.rodata,alloc,load,readonly,data,contents $< $@

${PROJECT}.bin: ${PROJECT}.elf
	arm-none-eabi-objcopy -Obinary $< $@

${PROJECT}.elf: ${OBJECTS}
	${CC} ${LDFLAGS} -o $@ $^ ${LDLIBS}

flash:
	${MAKE} ${PROJECT}.elf
	JlinkExe -device da14531 -if SWD -speed 4000 -autoconnect 1 -CommandFile commands.jlink

gdb-server:
	JlinkGDBServer -device da14531 -if SWD -speed 4000

jlink-cli:
	JlinkExe -device da14531 -if SWD -speed 4000 -autoconnect 1

run:
	arm-none-eabi-gdb -x jlink.gdb ${PROJECT}.elf

clean:
	rm -f *.o ${PROJECT}.elf ${PROJECT}.bin ${PROJECT}.o output.map

%.o : %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $(notdir $@)

%.o : %.S
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $(notdir $@)
