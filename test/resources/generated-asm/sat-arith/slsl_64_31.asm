@main_type "default"

@type default
  @method test_slsl_64_31(I64)I64
    @code
      @live.prim IR1
      slsli.64 IR1, IR1, 31
      ret.64 IR1
    @end
  @end
@end
