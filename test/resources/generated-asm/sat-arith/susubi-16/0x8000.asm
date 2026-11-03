@main_type "default"

@type default
  @method test_0x8000(I64)I64
    @code
      @live.prim IR1
      susubi.16 IR1, IR1, 0x8000
      ret.64 IR1
    @end
  @end
@end
