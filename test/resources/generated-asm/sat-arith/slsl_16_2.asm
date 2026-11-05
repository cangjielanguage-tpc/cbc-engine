@main_type "default"

@type default
  @method test_slsl_16_2(I64)I32
    @code
      @live.prim IR1
      slsli.16 IR1, IR1, 2
      ret.32 IR1
    @end
  @end
@end
