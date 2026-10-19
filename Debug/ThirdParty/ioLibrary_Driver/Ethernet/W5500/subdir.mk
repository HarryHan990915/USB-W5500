################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ThirdParty/ioLibrary_Driver/Ethernet/W5500/w5500.c 

OBJS += \
./ThirdParty/ioLibrary_Driver/Ethernet/W5500/w5500.o 

C_DEPS += \
./ThirdParty/ioLibrary_Driver/Ethernet/W5500/w5500.d 


# Each subdirectory must supply rules for building sources it contributes
ThirdParty/ioLibrary_Driver/Ethernet/W5500/%.o ThirdParty/ioLibrary_Driver/Ethernet/W5500/%.su ThirdParty/ioLibrary_Driver/Ethernet/W5500/%.cyclo: ../ThirdParty/ioLibrary_Driver/Ethernet/W5500/%.c ThirdParty/ioLibrary_Driver/Ethernet/W5500/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG -D_WIZCHIP_=W5500 -DUSE_FULL_ERROR_LIST=1 -c -I../ThirdParty/ioLibrary_Driver/Ethernet -I../ThirdParty/ioLibrary_Driver/Ethernet/W5500 -I../ThirdParty/ioLibrary_Driver/Internet/DHCP -I../ThirdParty/scpi-parser/libscpi/inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-ThirdParty-2f-ioLibrary_Driver-2f-Ethernet-2f-W5500

clean-ThirdParty-2f-ioLibrary_Driver-2f-Ethernet-2f-W5500:
	-$(RM) ./ThirdParty/ioLibrary_Driver/Ethernet/W5500/w5500.cyclo ./ThirdParty/ioLibrary_Driver/Ethernet/W5500/w5500.d ./ThirdParty/ioLibrary_Driver/Ethernet/W5500/w5500.o ./ThirdParty/ioLibrary_Driver/Ethernet/W5500/w5500.su

.PHONY: clean-ThirdParty-2f-ioLibrary_Driver-2f-Ethernet-2f-W5500

