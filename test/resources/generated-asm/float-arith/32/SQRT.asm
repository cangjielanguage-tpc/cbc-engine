@main_type "default"

@type default
  @method main()F32
    @code
      fsqrt.32 FR0, FR0
      fret.32 FR0
    @end
  @end
@end
