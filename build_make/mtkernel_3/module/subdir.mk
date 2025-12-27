################################################################################
# micro T-Kernel 3.0 BSP makefile
################################################################################

MODULE_SRCS = $(wildcard ../module/*.c)

OBJS += $(MODULE_SRCS:../module/%.c=./mtkernel_3/module/%.o)
C_DEPS += $(OBJS:.o=.d)

mtkernel_3/module/%.o: ../module/%.c
	@echo 'Building file: $<'
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) \
		-MMD -MP -MF"$(@:.o=.d)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

-include $(C_DEPS)
