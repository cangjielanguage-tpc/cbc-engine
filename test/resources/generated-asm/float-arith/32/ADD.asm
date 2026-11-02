@main_type "default"

@type default
  @method main()F32
    @code
      fadd.32 FR0, FR0, FR1
      fret.32 FR0
    @end
  @end
@end
