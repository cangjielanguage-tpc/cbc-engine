@main_type "default"

@type default
  @method test_suneg_8(I64)I64
    @code
      @live.prim IR1
      suneg.8 IR1, IR1
      ret.64 IR1
    @end
  @end
@end
