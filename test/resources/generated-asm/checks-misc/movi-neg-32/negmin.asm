@main_type "default"

@type default
  @method test_negmin()I64
    @code
      movi.32 IR1, -0x80000000
      ret.64 IR1
    @end
  @end
@end
