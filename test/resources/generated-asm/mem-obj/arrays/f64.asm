@main_type "default"

@type default
  @method test_f64(I64, I64, F64)F64
    @code
      @live.ref IR1
      @live.prim IR2
      st.arr.f64 FR0, IR1, IR2
      ld.arr.f64 FR1, IR1, IR2
      @dead IR1, IR2
      fret.64 FR1
    @end
  @end
@end
