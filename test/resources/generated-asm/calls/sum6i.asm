@main_type "default"

@method_ref default.sum6i = default@ref sum6i(I64, I64, I64, I64, I64, I64)I64

@type default
  @method sum6i(I64, I64, I64, I64, I64, I64)I64
    @code
      @live.prim IR1, IR2, IR3, IR4, IR5, IR6
      add.64 IR1, IR1, IR2
      add.64 IR1, IR1, IR3
      add.64 IR1, IR1, IR4
      add.64 IR1, IR1, IR5
      add.64 IR1, IR1, IR6
      ret.64 IR1
    @end
  @end

  @method main(I64)I64
    @code
      @live.prim IR1
      addi.64 IR2, IR1, 0x1
      addi.64 IR3, IR1, 0x2
      addi.64 IR4, IR1, 0x3
      addi.64 IR5, IR1, 0x4
      addi.64 IR6, IR1, 0x5
      call.direct #default.sum6i, IR1, IR2, IR3, IR4, IR5, IR6
      ret.64 IR1
    @end
  @end
@end
