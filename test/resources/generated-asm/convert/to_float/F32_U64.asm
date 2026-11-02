@main_type "default"

@type default
  @method main()I64
    @code
      @live.prim IR1
      i2f U64, F32, IR1, FR0
      fret.64 FR0
    @end
  @end
@end
