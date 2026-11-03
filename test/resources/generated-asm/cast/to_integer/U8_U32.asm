@main_type "default"

@type default
  @method test_U8_U32(U32)I64
    @code
      @live.prim IR1
      i2i U32, U8, IR1, IR1
      ret.64 IR1
    @end
  @end
@end
