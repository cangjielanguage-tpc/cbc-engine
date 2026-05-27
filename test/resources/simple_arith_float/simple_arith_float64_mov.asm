;strict
@main_type "default"

@type default
  @method test_MOV()F64
    @code
      fmov.64 FR0, FR1
      fret.64 FR0
    @end
  @end

  @method test_I2F()F64
    @code
      @live.prim IR1
      movi2f.64 FR0, IR1
      fret.64 FR0
    @end
  @end

  @method test_F2I()I64
    @code
      movf2i.64 IR1, FR0
      ret.64 IR1
    @end
  @end
@end
