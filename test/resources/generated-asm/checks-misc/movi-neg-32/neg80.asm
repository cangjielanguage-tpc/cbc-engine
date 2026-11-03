@main_type "default"

@type default
  @method test_neg80()I64
    @code
      movi.32 IR1, -0x80
      ret.64 IR1
    @end
  @end
@end
