@main_type "default"

@type default
  @method test_0x0(I64)I64
    @code
      @live.prim IR1
      casri.32 IR1, IR1, 0x0
      ret.64 IR1
    @end
  @end
@end
