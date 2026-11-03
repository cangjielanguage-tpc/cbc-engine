@main_type "default"

@type default
  @method test_U64_F64(F64)I64
    @code
      cf2i F64, U64, FR0, IR1
      ret.64 IR1
    @end
  @end
@end
