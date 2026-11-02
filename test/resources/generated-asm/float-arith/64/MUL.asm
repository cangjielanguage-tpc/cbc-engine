@main_type "default"

@type default
  @method main()F64
    @code
      fmul.64 FR0, FR0, FR1
      fret.64 FR0
    @end
  @end
@end
