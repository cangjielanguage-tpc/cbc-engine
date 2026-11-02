@main_type "default"

@type default
  @method test_0x10(I64)I64
    @code
      @live.prim IR1
      lsli.64 IR1, IR1, 0x10
      ret.64 IR1
    @end
  @end
@end
