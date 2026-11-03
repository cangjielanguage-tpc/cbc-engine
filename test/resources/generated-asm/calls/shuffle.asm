@main_type "default"

@method_ref default.pick3 = default@ref pick3(I64, I64, I64)I64

@type default
  @method pick3(I64, I64, I64)I64
    @code
      @live.prim IR1, IR2, IR3
      muli.64 IR1, IR1, 0x100
      muli.64 IR2, IR2, 0x10
      add.64 IR1, IR1, IR2
      add.64 IR1, IR1, IR3
      ret.64 IR1
    @end
  @end

  @method main(I64)I64
    @code
      @live.prim IR1
      addi.64 IR2, IR1, 0x1
      addi.64 IR3, IR1, 0x2
      call.direct #default.pick3, IR3, IR2, IR1
      ret.64 IR1
    @end
  @end
@end
