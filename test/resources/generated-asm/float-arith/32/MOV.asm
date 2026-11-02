@main_type "default"

@type default
  @method test_MOV(F32, F32)F32
    @code
      fmov.32 FR0, FR1
      fret.32 FR0
    @end
  @end
@end
