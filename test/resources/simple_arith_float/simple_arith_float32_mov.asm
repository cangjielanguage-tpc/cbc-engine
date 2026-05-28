;strict
@main_type "default"

@type default
  @method test_MOV()F32
    @code
      fmov.32 FR0, FR1
      fret.32 FR0
    @end
  @end

  @method test_I2F()F32
    @code
      @live.prim IR1
      movi2f.32 FR0, IR1
      fret.32 FR0
    @end
  @end

  @method test_F2I()I32
    @code
      movf2i.32 IR1, FR0
      ret.32 IR1
    @end
  @end
@end
