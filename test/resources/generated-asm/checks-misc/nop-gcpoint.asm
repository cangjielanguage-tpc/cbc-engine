@main_type "default"

@type default
  @method test_nop_gcpoint(I64)I64
    @code
      nop
      gcpoint
      movi.64 IR1, 0x3
      nop
      gcpoint
      ret.64 IR1
    @end
  @end
@end
