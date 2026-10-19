################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ThirdParty/scpi-parser/libscpi/src/error.c \
../ThirdParty/scpi-parser/libscpi/src/expression.c \
../ThirdParty/scpi-parser/libscpi/src/fifo.c \
../ThirdParty/scpi-parser/libscpi/src/ieee488.c \
../ThirdParty/scpi-parser/libscpi/src/lexer.c \
../ThirdParty/scpi-parser/libscpi/src/minimal.c \
../ThirdParty/scpi-parser/libscpi/src/parser.c \
../ThirdParty/scpi-parser/libscpi/src/units.c \
../ThirdParty/scpi-parser/libscpi/src/utils.c 

OBJS += \
./ThirdParty/scpi-parser/libscpi/src/error.o \
./ThirdParty/scpi-parser/libscpi/src/expression.o \
./ThirdParty/scpi-parser/libscpi/src/fifo.o \
./ThirdParty/scpi-parser/libscpi/src/ieee488.o \
./ThirdParty/scpi-parser/libscpi/src/lexer.o \
./ThirdParty/scpi-parser/libscpi/src/minimal.o \
./ThirdParty/scpi-parser/libscpi/src/parser.o \
./ThirdParty/scpi-parser/libscpi/src/units.o \
./ThirdParty/scpi-parser/libscpi/src/utils.o 

C_DEPS += \
./ThirdParty/scpi-parser/libscpi/src/error.d \
./ThirdParty/scpi-parser/libscpi/src/expression.d \
./ThirdParty/scpi-parser/libscpi/src/fifo.d \
./ThirdParty/scpi-parser/libscpi/src/ieee488.d \
./ThirdParty/scpi-parser/libscpi/src/lexer.d \
./ThirdParty/scpi-parser/libscpi/src/minimal.d \
./ThirdParty/scpi-parser/libscpi/src/parser.d \
./ThirdParty/scpi-parser/libscpi/src/units.d \
./ThirdParty/scpi-parser/libscpi/src/utils.d 


# Each subdirectory must supply rules for building sources it contributes
ThirdParty/scpi-parser/libscpi/src/%.o ThirdParty/scpi-parser/libscpi/src/%.su ThirdParty/scpi-parser/libscpi/src/%.cyclo: ../ThirdParty/scpi-parser/libscpi/src/%.c ThirdParty/scpi-parser/libscpi/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG -D_WIZCHIP_=W5500 -DUSE_FULL_ERROR_LIST=1 -c -I../ThirdParty/ioLibrary_Driver/Ethernet -I../ThirdParty/ioLibrary_Driver/Ethernet/W5500 -I../ThirdParty/ioLibrary_Driver/Internet/DHCP -I../ThirdParty/scpi-parser/libscpi/inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-ThirdParty-2f-scpi-2d-parser-2f-libscpi-2f-src

clean-ThirdParty-2f-scpi-2d-parser-2f-libscpi-2f-src:
	-$(RM) ./ThirdParty/scpi-parser/libscpi/src/error.cyclo ./ThirdParty/scpi-parser/libscpi/src/error.d ./ThirdParty/scpi-parser/libscpi/src/error.o ./ThirdParty/scpi-parser/libscpi/src/error.su ./ThirdParty/scpi-parser/libscpi/src/expression.cyclo ./ThirdParty/scpi-parser/libscpi/src/expression.d ./ThirdParty/scpi-parser/libscpi/src/expression.o ./ThirdParty/scpi-parser/libscpi/src/expression.su ./ThirdParty/scpi-parser/libscpi/src/fifo.cyclo ./ThirdParty/scpi-parser/libscpi/src/fifo.d ./ThirdParty/scpi-parser/libscpi/src/fifo.o ./ThirdParty/scpi-parser/libscpi/src/fifo.su ./ThirdParty/scpi-parser/libscpi/src/ieee488.cyclo ./ThirdParty/scpi-parser/libscpi/src/ieee488.d ./ThirdParty/scpi-parser/libscpi/src/ieee488.o ./ThirdParty/scpi-parser/libscpi/src/ieee488.su ./ThirdParty/scpi-parser/libscpi/src/lexer.cyclo ./ThirdParty/scpi-parser/libscpi/src/lexer.d ./ThirdParty/scpi-parser/libscpi/src/lexer.o ./ThirdParty/scpi-parser/libscpi/src/lexer.su ./ThirdParty/scpi-parser/libscpi/src/minimal.cyclo ./ThirdParty/scpi-parser/libscpi/src/minimal.d ./ThirdParty/scpi-parser/libscpi/src/minimal.o ./ThirdParty/scpi-parser/libscpi/src/minimal.su ./ThirdParty/scpi-parser/libscpi/src/parser.cyclo ./ThirdParty/scpi-parser/libscpi/src/parser.d ./ThirdParty/scpi-parser/libscpi/src/parser.o ./ThirdParty/scpi-parser/libscpi/src/parser.su ./ThirdParty/scpi-parser/libscpi/src/units.cyclo ./ThirdParty/scpi-parser/libscpi/src/units.d ./ThirdParty/scpi-parser/libscpi/src/units.o ./ThirdParty/scpi-parser/libscpi/src/units.su ./ThirdParty/scpi-parser/libscpi/src/utils.cyclo ./ThirdParty/scpi-parser/libscpi/src/utils.d ./ThirdParty/scpi-parser/libscpi/src/utils.o ./ThirdParty/scpi-parser/libscpi/src/utils.su

.PHONY: clean-ThirdParty-2f-scpi-2d-parser-2f-libscpi-2f-src

