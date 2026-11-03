@main_type "default"

@type default
  @method test_FNLT(F32, F32)I64
    @code
      movi.32 IR1, 0x1
      fbcc.32 FNLT, FR0, FR1, skip
      subi.32 IR1, IR1, 0x1
      skip:
      ret.64 IR1
    @end
  @end
@end
