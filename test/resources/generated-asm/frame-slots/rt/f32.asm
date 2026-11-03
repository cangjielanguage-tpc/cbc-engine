@main_type "default"

@type default
  @method test_f32(F32)F32
    @untyped_count 0x1
    @code
      st.uslot.f32 FR0, $0
      ld.uslot.f32 FR0, $0
      fret.32 FR0
    @end
  @end
@end
