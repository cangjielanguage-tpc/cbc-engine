@main_type "default"

@type default
  @method test_LOG2(F32)F32
    @code
      flog2.32 FR0, FR0
      fret.32 FR0
    @end
  @end
@end
