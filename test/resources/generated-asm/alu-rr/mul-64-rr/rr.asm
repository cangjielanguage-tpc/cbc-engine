@main_type "default"

@type default
  @method test_mul_64(I64, I64)I64
    @code
      @live.prim IR1, IR2
      mul.64 IR1, IR1, IR2
      ret.64 IR1
    @end
  @end
@end
