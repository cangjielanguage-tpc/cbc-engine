@main_type "default"

@type default
  @method test_negmin()I64
    @code
      movi.64 IR1, -0x8000000000000000
      ret.64 IR1
    @end
  @end
@end
