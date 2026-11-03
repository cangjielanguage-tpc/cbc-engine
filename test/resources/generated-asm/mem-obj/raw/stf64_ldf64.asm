@main_type "default"

@type default
  @method test_stf64_ldf64(I64, F64)F64
    @code
      @live.prim IR1
      st.raw.f64 FR0, IR1, 0x8
      ld.raw.f64 FR1, IR1, 0x8
      @dead IR1
      fret.64 FR1
    @end
  @end
@end
