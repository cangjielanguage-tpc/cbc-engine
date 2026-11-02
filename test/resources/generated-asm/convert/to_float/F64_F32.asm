@main_type "default"

@type default
  @method main()I64
    @code
      @live.prim IR1
      f2f F32, F64, FR0, FR0
      fret.64 FR0
    @end
  @end
@end
