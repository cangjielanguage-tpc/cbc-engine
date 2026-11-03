@main_type "default"

@type default
  @method test_movi32_wide()I64
    @code
      movi.32 IR1, 0x12345678
      ret.64 IR1
    @end
  @end
@end
