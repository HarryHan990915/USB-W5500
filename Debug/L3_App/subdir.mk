################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../L3_App/app_main.c \
../L3_App/app_tasks.c \
../L3_App/led_bench.c \
../L3_App/scpi_commands.c 

OBJS += \
./L3_App/app_main.o \
./L3_App/app_tasks.o \
./L3_App/led_bench.o \
./L3_App/scpi_commands.o 

C_DEPS += \
./L3_App/app_main.d \
./L3_App/app_tasks.d \
./L3_App/led_bench.d \
./L3_App/scpi_commands.d 


# Each subdirectory must supply rules for building sources it contributes
L3_App/%.o L3_App/%.su L3_App/%.cyclo: ../L3_App/%.c L3_App/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G071xx -DUSE_FULL_ERROR_LIST=1 -c -I../L3_App -I../L2_Service -I../L1_Driver -I../ThirdParty/scpi-parser/libscpi/inc -I../Core/Inc -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Drivers/CMSIS/Device/ST/STM32G0xx/Include -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Drivers/CMSIS/Include -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Middlewares/Third_Party/FreeRTOS/Source/include -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM0 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-L3_App

clean-L3_App:
	-$(RM) ./L3_App/app_main.cyclo ./L3_App/app_main.d ./L3_App/app_main.o ./L3_App/app_main.su ./L3_App/app_tasks.cyclo ./L3_App/app_tasks.d ./L3_App/app_tasks.o ./L3_App/app_tasks.su ./L3_App/led_bench.cyclo ./L3_App/led_bench.d ./L3_App/led_bench.o ./L3_App/led_bench.su ./L3_App/scpi_commands.cyclo ./L3_App/scpi_commands.d ./L3_App/scpi_commands.o ./L3_App/scpi_commands.su

.PHONY: clean-L3_App

