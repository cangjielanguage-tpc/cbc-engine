@main_type "default"

@type default
  @method test_f32(I64, I64, F32)F32
    @code
      @live.ref IR1
      @live.prim IR2
      st.arr.f32 FR0, IR1, IR2
      ld.arr.f32 FR1, IR1, IR2
      @dead IR1, IR2
      fret.32 FR1
    @end
  @end
@end
