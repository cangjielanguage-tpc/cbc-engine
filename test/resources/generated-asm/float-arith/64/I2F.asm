@main_type "default"

@type default
  @method main()F64
    @code
      @live.prim IR1
      movi2f.64 FR0, IR1
      fret.64 FR0
    @end
  @end
@end
