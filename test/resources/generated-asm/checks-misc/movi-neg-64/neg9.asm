@main_type "default"

@type default
  @method test_neg9()I64
    @code
      movi.64 IR1, -0x9
      ret.64 IR1
    @end
  @end
@end
