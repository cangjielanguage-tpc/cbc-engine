@main_type "default"

@type default
  @method test_0x100200(I64)I64
    @code
      @live.prim IR1
      andi.64 IR1, IR1, 0x100200
      ret.64 IR1
    @end
  @end
@end
