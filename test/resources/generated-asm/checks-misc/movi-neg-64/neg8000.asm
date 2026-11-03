@main_type "default"

@type default
  @method test_neg8000()I64
    @code
      movi.64 IR1, -0x8000
      ret.64 IR1
    @end
  @end
@end
