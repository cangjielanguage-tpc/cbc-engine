@main_type "default"

@type default
  @method main()F64
    @code
      fabs.64 FR0, FR0
      fret.64 FR0
    @end
  @end
@end
