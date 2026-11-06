@main_type "default"

@type default
  @method test_slsl_32_31(I64)I32
    @code
      @live.prim IR1
      slsli.32 IR1, IR1, 31
      ret.32 IR1
    @end
  @end
@end
