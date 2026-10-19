################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../Core/Startup/startup_stm32g071rbtx.s 

OBJS += \
./Core/Startup/startup_stm32g071rbtx.o 

S_DEPS += \
./Core/Startup/startup_stm32g071rbtx.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Startup/%.o: ../Core/Startup/%.s Core/Startup/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m0plus -g3 -DDEBUG -c -I../Core/Inc -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Drivers/STM32G0xx_HAL_Driver/Inc -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Drivers/STM32G0xx_HAL_Driver/Inc/Legacy -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Middlewares/Third_Party/FreeRTOS/Source/include -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM0 -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Drivers/CMSIS/Device/ST/STM32G0xx/Include -IC:/Users/HarryHan/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.3/Drivers/CMSIS/Include -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@" "$<"

clean: clean-Core-2f-Startup

clean-Core-2f-Startup:
	-$(RM) ./Core/Startup/startup_stm32g071rbtx.d ./Core/Startup/startup_stm32g071rbtx.o

.PHONY: clean-Core-2f-Startup

