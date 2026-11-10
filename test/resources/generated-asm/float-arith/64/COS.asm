@main_type "default"

@type default
  @method test_COS(F64)F64
    @code
      fcos.64 FR0, FR0
      fret.64 FR0
    @end
  @end
@end
