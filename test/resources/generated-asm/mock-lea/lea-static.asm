@main_type "default"
@field_ref stat0 = default@ref s0 I64
@field_ref stat1 = default@ref s1 I64

@type default
  @field s0 I64
    @flags STATIC
  @end
  @field s1 I64
    @flags STATIC
  @end

  @method test_lea_static_base()I64
    @code
      lea.static IR1, IR2, #stat0
      ret.ref IR2
    @end
  @end

  @method test_lea_static_rt(I64)I64
    @code
      @live.prim IR1
      lea.static IR2, IR3, #stat0
      @dead IR3
      st.raw.64 IR1, IR2, 0x0
      ld.static IR4, #stat0
      scc.64 EQ, IR5, IR4, IR1
      @dead IR1, IR2, IR4
      ret.64 IR5
    @end
  @end

  @method test_static_pair(I64, I64)I64
    @code
      @live.prim IR1, IR2
      st.static IR1, #stat0
      st.static IR2, #stat1
      ld.static IR3, #stat1
      ld.static IR4, #stat0
      scc.64 EQ, IR5, IR3, IR2
      scc.64 EQ, IR6, IR4, IR1
      mul.64 IR5, IR5, IR6
      @dead IR1, IR2, IR3, IR4, IR6
      ret.64 IR5
    @end
  @end
@end
