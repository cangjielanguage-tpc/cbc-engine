@main_type "default"

@method_ref default.twice = default@ref twice(I64)I64

@type default
  @method twice(I64)I64
    @code
      @live.prim IR1
      add.64 IR1, IR1, IR1
      ret.64 IR1
    @end
  @end

  @method main(I64)I64
    @code
      @live.prim IR1
      call.direct #default.twice, IR1
      add.64 IR2, IR1, IR1
      addi.64 IR3, IR2, 0x1
      call.direct #default.twice, IR3
      ret.64 IR1
    @end
  @end
@end
