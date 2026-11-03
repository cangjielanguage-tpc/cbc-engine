@main_type "default"

@type default
  @method test_neg1()I64
    @code
      movi.32 IR1, -0x1
      ret.64 IR1
    @end
  @end
@end
