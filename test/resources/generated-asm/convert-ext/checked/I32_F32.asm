@main_type "default"

@type default
  @method test_I32_F32(F32)I64
    @code
      cf2i F32, I32, FR0, IR1
      ret.64 IR1
    @end
  @end
@end
