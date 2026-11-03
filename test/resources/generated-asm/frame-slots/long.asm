@main_type "default"

@type default
  @method test_long(I64)I64
    @untyped_count 0x104
    @code
      @live.prim IR1
      st.uslot.64 IR1, $0
      ld.uslot.64 IR2, $0
      @dead IR1
      st.uslot.64 IR2, $255
      ld.uslot.64 IR3, $255
      add.64 IR2, IR2, IR3
      @dead IR3, $255, $0
      ret.64 IR2
    @end
  @end

  @method test_long_f(F64)F64
    @untyped_count 0x104
    @code
      st.uslot.f64 FR0, $0
      ld.uslot.f64 FR0, $0
      fret.64 FR0
    @end
  @end
@end
