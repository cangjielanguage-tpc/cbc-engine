@main_type "default"

@type default
  @method test_movbp_local()I64
    @code
      movbp.local IR1
      @live.prim IR1
      ret.64 IR1
    @end
  @end

  @method test_movbp_global()I64
    @code
      movbp.global IR1
      @live.prim IR1
      ret.64 IR1
    @end
  @end

  @method test_movbp_distinct()I64
    @code
      movbp.local IR1
      @live.prim IR1
      movbp.global IR2
      @live.prim IR2
      scc.64 NE, IR3, IR1, IR2
      @dead IR1, IR2
      ret.64 IR3
    @end
  @end

  @method test_movbp_local_read(I64)I64
    @code
      @live.prim IR1
      movbp.local IR2
      @live.ref IR2
      movi.64 IR9, 0x1000
      ld.g IR4, IR2, IR1, IR9
      @dead IR1, IR2, IR9
      ret.ref IR4
    @end
  @end

  @method test_movbp_global_read(I64)I64
    @code
      @live.prim IR1
      movbp.global IR2
      @live.ref IR2
      movi.64 IR9, 0x1000
      ld.g IR4, IR2, IR1, IR9
      @dead IR1, IR2, IR9
      ret.ref IR4
    @end
  @end

  @method test_movbp_local_write(I64, I64)I64
    @code
      @live.prim IR1
      @live.ref IR2
      movbp.local IR3
      @live.ref IR3
      movi.64 IR9, 0x1000
      st.g IR2, IR3, IR1, IR9
      ld.g IR5, IR3, IR1, IR9
      @dead IR1, IR2, IR3, IR9
      ret.ref IR5
    @end
  @end

  @method test_movbp_global_write(I64, I64)I64
    @code
      @live.prim IR1
      @live.ref IR2
      movbp.global IR3
      @live.ref IR3
      movi.64 IR9, 0x1000
      st.g IR2, IR3, IR1, IR9
      ld.g IR5, IR3, IR1, IR9
      @dead IR1, IR2, IR3, IR9
      ret.ref IR5
    @end
  @end
@end
