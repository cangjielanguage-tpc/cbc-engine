@main_type "default"

@type default
  @method test_sneg_64(I64)I64
    @code
      @live.prim IR1
      sneg.64 IR1, IR1
      ret.64 IR1
    @end
  @end
@end
