@main_type "default"

@type default
  @method test_f64(F64)F64
    @untyped_count 0x1
    @code
      st.uslot.f64 FR0, $0
      ld.uslot.f64 FR0, $0
      fret.64 FR0
    @end
  @end
@end
