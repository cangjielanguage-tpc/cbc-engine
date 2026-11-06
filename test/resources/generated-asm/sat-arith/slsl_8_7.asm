@main_type "default"

@type default
  @method test_slsl_8_7(I64)I32
    @code
      @live.prim IR1
      slsli.8 IR1, IR1, 7
      ret.32 IR1
    @end
  @end
@end
