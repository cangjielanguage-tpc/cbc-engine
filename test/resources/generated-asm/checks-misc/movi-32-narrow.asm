@main_type "default"

@type default
  @method test_movi32_narrow()I64
    @code
      movi.32 IR1, 0x7ff
      ret.64 IR1
    @end
  @end
@end
