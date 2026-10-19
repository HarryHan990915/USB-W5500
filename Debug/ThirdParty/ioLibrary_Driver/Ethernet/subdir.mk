################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ThirdParty/ioLibrary_Driver/Ethernet/socket.c \
../ThirdParty/ioLibrary_Driver/Ethernet/wizchip_conf.c 

OBJS += \
./ThirdParty/ioLibrary_Driver/Ethernet/socket.o \
./ThirdParty/ioLibrary_Driver/Ethernet/wizchip_conf.o 

C_DEPS += \
./ThirdParty/ioLibrary_Driver/Ethernet/socket.d \
./ThirdParty/ioLibrary_Driver/Ethernet/wizchip_conf.d 


# Each subdirectory must supply rules for building sources it contributes
ThirdParty/ioLibrary_Driver/Ethernet/%.o ThirdParty/ioLibrary_Driver/Ethernet/%.su ThirdParty/ioLibrary_Driver/Ethernet/%.cyclo: ../ThirdParty/ioLibrary_Driver/Ethernet/%.c ThirdParty/ioLibrary_Driver/Ethernet/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG -D_WIZCHIP_=W5500 -DUSE_FULL_ERROR_LIST=1 -c -I../ThirdParty/ioLibrary_Driver/Ethernet -I../ThirdParty/ioLibrary_Driver/Ethernet/W5500 -I../ThirdParty/ioLibrary_Driver/Internet/DHCP -I../ThirdParty/scpi-parser/libscpi/inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-ThirdParty-2f-ioLibrary_Driver-2f-Ethernet

clean-ThirdParty-2f-ioLibrary_Driver-2f-Ethernet:
	-$(RM) ./ThirdParty/ioLibrary_Driver/Ethernet/socket.cyclo ./ThirdParty/ioLibrary_Driver/Ethernet/socket.d ./ThirdParty/ioLibrary_Driver/Ethernet/socket.o ./ThirdParty/ioLibrary_Driver/Ethernet/socket.su ./ThirdParty/ioLibrary_Driver/Ethernet/wizchip_conf.cyclo ./ThirdParty/ioLibrary_Driver/Ethernet/wizchip_conf.d ./ThirdParty/ioLibrary_Driver/Ethernet/wizchip_conf.o ./ThirdParty/ioLibrary_Driver/Ethernet/wizchip_conf.su

.PHONY: clean-ThirdParty-2f-ioLibrary_Driver-2f-Ethernet

