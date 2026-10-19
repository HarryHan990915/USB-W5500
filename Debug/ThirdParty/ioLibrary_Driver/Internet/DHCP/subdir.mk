################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ThirdParty/ioLibrary_Driver/Internet/DHCP/dhcp.c 

OBJS += \
./ThirdParty/ioLibrary_Driver/Internet/DHCP/dhcp.o 

C_DEPS += \
./ThirdParty/ioLibrary_Driver/Internet/DHCP/dhcp.d 


# Each subdirectory must supply rules for building sources it contributes
ThirdParty/ioLibrary_Driver/Internet/DHCP/%.o ThirdParty/ioLibrary_Driver/Internet/DHCP/%.su ThirdParty/ioLibrary_Driver/Internet/DHCP/%.cyclo: ../ThirdParty/ioLibrary_Driver/Internet/DHCP/%.c ThirdParty/ioLibrary_Driver/Internet/DHCP/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG -D_WIZCHIP_=W5500 -DUSE_FULL_ERROR_LIST=1 -c -I../ThirdParty/ioLibrary_Driver/Ethernet -I../ThirdParty/ioLibrary_Driver/Ethernet/W5500 -I../ThirdParty/ioLibrary_Driver/Internet/DHCP -I../ThirdParty/scpi-parser/libscpi/inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-ThirdParty-2f-ioLibrary_Driver-2f-Internet-2f-DHCP

clean-ThirdParty-2f-ioLibrary_Driver-2f-Internet-2f-DHCP:
	-$(RM) ./ThirdParty/ioLibrary_Driver/Internet/DHCP/dhcp.cyclo ./ThirdParty/ioLibrary_Driver/Internet/DHCP/dhcp.d ./ThirdParty/ioLibrary_Driver/Internet/DHCP/dhcp.o ./ThirdParty/ioLibrary_Driver/Internet/DHCP/dhcp.su

.PHONY: clean-ThirdParty-2f-ioLibrary_Driver-2f-Internet-2f-DHCP

