@main_type "default"

@type default
  @method test_MUL(F32, F32)F32
    @code
      fmul.32 FR0, FR0, FR1
      fret.32 FR0
    @end
  @end
@end
