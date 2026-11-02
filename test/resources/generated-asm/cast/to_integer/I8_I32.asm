@main_type "default"

@type default
  @method test_I8_I32(I32)I64
    @code
      @live.prim IR1
      i2i I32, I8, IR1, IR1
      ret.64 IR1
    @end
  @end
@end
