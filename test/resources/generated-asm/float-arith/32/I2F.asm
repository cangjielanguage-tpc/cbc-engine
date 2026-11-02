@main_type "default"

@type default
  @method test_I2F(I32)F32
    @code
      @live.prim IR1
      movi2f.32 FR0, IR1
      fret.32 FR0
    @end
  @end
@end
