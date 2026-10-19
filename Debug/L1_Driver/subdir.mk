################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../L1_Driver/flash_store.c \
../L1_Driver/sys_time.c \
../L1_Driver/test_led.c \
../L1_Driver/uart_log.c \
../L1_Driver/w5500_port.c 

OBJS += \
./L1_Driver/flash_store.o \
./L1_Driver/sys_time.o \
./L1_Driver/test_led.o \
./L1_Driver/uart_log.o \
./L1_Driver/w5500_port.o 

C_DEPS += \
./L1_Driver/flash_store.d \
./L1_Driver/sys_time.d \
./L1_Driver/test_led.d \
./L1_Driver/uart_log.d \
./L1_Driver/w5500_port.d 


# Each subdirectory must supply rules for building sources it contributes
L1_Driver/%.o L1_Driver/%.su L1_Driver/%.cyclo: ../L1_Driver/%.c L1_Driver/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G071xx -D_WIZCHIP_=W5500 -DUSE_FULL_ERROR_LIST=1 -DTESTER_IO_SIM=1 -c -I../L1_Driver -I../Core/Inc -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Drivers/STM32G0xx_HAL_Driver/Inc -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Drivers/STM32G0xx_HAL_Driver/Inc/Legacy -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Drivers/CMSIS/Device/ST/STM32G0xx/Include -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Drivers/CMSIS/Include -I../ThirdParty/ioLibrary_Driver -I../ThirdParty/ioLibrary_Driver/Ethernet -I../ThirdParty/ioLibrary_Driver/Ethernet/W5500 -I../ThirdParty/ioLibrary_Driver/Internet/DHCP -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-L1_Driver

clean-L1_Driver:
	-$(RM) ./L1_Driver/flash_store.cyclo ./L1_Driver/flash_store.d ./L1_Driver/flash_store.o ./L1_Driver/flash_store.su ./L1_Driver/sys_time.cyclo ./L1_Driver/sys_time.d ./L1_Driver/sys_time.o ./L1_Driver/sys_time.su ./L1_Driver/test_led.cyclo ./L1_Driver/test_led.d ./L1_Driver/test_led.o ./L1_Driver/test_led.su ./L1_Driver/uart_log.cyclo ./L1_Driver/uart_log.d ./L1_Driver/uart_log.o ./L1_Driver/uart_log.su ./L1_Driver/w5500_port.cyclo ./L1_Driver/w5500_port.d ./L1_Driver/w5500_port.o ./L1_Driver/w5500_port.su

.PHONY: clean-L1_Driver

