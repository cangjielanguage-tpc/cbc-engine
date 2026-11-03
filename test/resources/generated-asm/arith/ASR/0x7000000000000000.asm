@main_type "default"

@type default
  @method test_0x7000000000000000(I64)I64
    @code
      @live.prim IR1
      asri.64 IR1, IR1, 0x7000000000000000
      ret.64 IR1
    @end
  @end
@end
