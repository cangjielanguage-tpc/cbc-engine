@main_type "default"

@type default
  @method test_nullcheck(I64)I64
    @code
      @live.ref IR1
      nullcheck IR1
      @dead IR1
      nop
      gcpoint
      movi.64 IR1, 0x1
      ret.64 IR1
    @end
  @end
@end
