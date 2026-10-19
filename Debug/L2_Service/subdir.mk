################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../L2_Service/config_service.c \
../L2_Service/diag_service.c \
../L2_Service/net_service.c \
../L2_Service/scpi_server.c \
../L2_Service/string_service.c 

OBJS += \
./L2_Service/config_service.o \
./L2_Service/diag_service.o \
./L2_Service/net_service.o \
./L2_Service/scpi_server.o \
./L2_Service/string_service.o 

C_DEPS += \
./L2_Service/config_service.d \
./L2_Service/diag_service.d \
./L2_Service/net_service.d \
./L2_Service/scpi_server.d \
./L2_Service/string_service.d 


# Each subdirectory must supply rules for building sources it contributes
L2_Service/%.o L2_Service/%.su L2_Service/%.cyclo: ../L2_Service/%.c L2_Service/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G071xx -D_WIZCHIP_=W5500 -DUSE_FULL_ERROR_LIST=1 -c -I../L2_Service -I../L1_Driver -I../ThirdParty/scpi-parser/libscpi/inc -I../ThirdParty/ioLibrary_Driver/Ethernet -I../ThirdParty/ioLibrary_Driver/Ethernet/W5500 -I../ThirdParty/ioLibrary_Driver/Internet/DHCP -I../Core/Inc -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Drivers/CMSIS/Device/ST/STM32G0xx/Include -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Drivers/CMSIS/Include -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Middlewares/Third_Party/FreeRTOS/Source/include -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM0 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-L2_Service

clean-L2_Service:
	-$(RM) ./L2_Service/config_service.cyclo ./L2_Service/config_service.d ./L2_Service/config_service.o ./L2_Service/config_service.su ./L2_Service/diag_service.cyclo ./L2_Service/diag_service.d ./L2_Service/diag_service.o ./L2_Service/diag_service.su ./L2_Service/net_service.cyclo ./L2_Service/net_service.d ./L2_Service/net_service.o ./L2_Service/net_service.su ./L2_Service/scpi_server.cyclo ./L2_Service/scpi_server.d ./L2_Service/scpi_server.o ./L2_Service/scpi_server.su ./L2_Service/string_service.cyclo ./L2_Service/string_service.d ./L2_Service/string_service.o ./L2_Service/string_service.su

.PHONY: clean-L2_Service

