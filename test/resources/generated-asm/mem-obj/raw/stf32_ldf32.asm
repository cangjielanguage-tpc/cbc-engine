@main_type "default"

@type default
  @method test_stf32_ldf32(I64, F32)F32
    @code
      @live.prim IR1
      st.raw.f32 FR0, IR1, 0x8
      ld.raw.f32 FR1, IR1, 0x8
      @dead IR1
      fret.32 FR1
    @end
  @end
@end
