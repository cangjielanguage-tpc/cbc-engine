@main_type "default"

@method_ref default.pick2 = default@ref pick2(I64, F64)I64

@type default
  @method pick2(I64, F64)I64
    @code
      @live.prim IR1
      f2i F64, I64, FR0, IR2
      add.64 IR1, IR1, IR2
      ret.64 IR1
    @end
  @end

  @method main(I64)I64
    @untyped_count 0x2
    @code
      @live.prim IR1
      st.uslot.64 IR1, $0
      fmovi.64 FR0, 7.5
      st.uslot.f64 FR0, $1
      call.direct #default.pick2, $0, $1
      ret.64 IR1
    @end
  @end
@end
