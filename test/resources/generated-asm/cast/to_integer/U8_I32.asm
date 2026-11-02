@main_type "default"

@type default
  @method test_U8_I32(I32)I64
    @code
      @live.prim IR1
      i2i I32, U8, IR1, IR1
      ret.64 IR1
    @end
  @end
@end
