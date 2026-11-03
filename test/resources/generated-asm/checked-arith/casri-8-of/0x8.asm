@main_type "default"

@type default
  @method test_0x8(I64)I64
    @code
      @live.prim IR1
      casri.8 IR1, IR1, 0x8
      ret.64 IR1
    @end
  @end
@end
