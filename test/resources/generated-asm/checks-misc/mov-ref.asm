@main_type "default"

@type default
  @method test_movref(I64)I64
    @code
      @live.ref IR1
      mov.ref IR2, IR1
      @dead IR1
      nop
      gcpoint
      ret.ref IR2
    @end
  @end
@end
