@main_type "default"

@type default
  @method test_0x10000(I64)I64
    @code
      @live.prim IR1
      subi.64 IR1, IR1, 0x10000
      ret.64 IR1
    @end
  @end
@end
