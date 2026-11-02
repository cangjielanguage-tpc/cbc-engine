@main_type "default"

@type default
  @method test_U16_F64(F64)I64
    @code
      f2i F64, U16, FR0, IR1
      ret.64 IR1
    @end
  @end
@end
