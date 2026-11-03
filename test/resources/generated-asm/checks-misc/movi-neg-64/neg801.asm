@main_type "default"

@type default
  @method test_neg801()I64
    @code
      movi.64 IR1, -0x801
      ret.64 IR1
    @end
  @end
@end
