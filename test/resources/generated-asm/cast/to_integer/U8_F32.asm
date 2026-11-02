@main_type "default"

@type default
  @method test_U8_F32(F32)I64
    @code
      f2i F32, U8, FR0, IR1
      ret.64 IR1
    @end
  @end
@end
