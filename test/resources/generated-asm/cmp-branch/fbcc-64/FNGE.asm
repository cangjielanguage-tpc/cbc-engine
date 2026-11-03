@main_type "default"

@type default
  @method test_FNGE(F64, F64)I64
    @code
      movi.64 IR1, 0x1
      fbcc.64 FNGE, FR0, FR1, skip
      subi.64 IR1, IR1, 0x1
      skip:
      ret.64 IR1
    @end
  @end
@end
