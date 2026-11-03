@main_type "default"

@type default
  @method test_0xfe(I64)I64
    @code
      @live.prim IR1
      cdivi.8 IR1, IR1, 0xfe
      ret.64 IR1
    @end
  @end
@end
