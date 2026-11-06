@main_type "default"
@field_ref obj_x = mem:Obj@ref x I64
@type mem:Rec
  @flags PUBLIC RECORD
  @field f0 I64
    @flags PUBLIC
  @end
@end

@type mem:Obj
  @field x I64
    @flags PUBLIC
  @end
@end

@type default
  @method test_copy_local(I64, I64)I64
    @code
      @live.prim IR1, IR2
      mov.64 IR5, IR1
      mov.64 IR6, IR2
      @dead IR1, IR2
      newobj mem:Rec@rec
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR3, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR4, IR1
      @dead IR1
      movi.64 IR7, 0xfeed
      st.raw.64 IR5, IR2, 0x0
      st.raw.64 IR6, IR2, 0x8
      st.raw.64 IR7, IR4, 0x0
      @dead IR2, IR3
      @live.rec IR2, IR3
      copy.local IR3, IR2, mem:Rec@rec
      ld.raw.64 IR8, IR3, 0x0
      scc.64 EQ, IR8, IR8, IR5
      ld.raw.64 IR9, IR3, 0x8
      scc.64 EQ, IR9, IR9, IR6
      ld.raw.64 IR1, IR4, 0x0
      scc.64 EQ, IR1, IR1, IR7
      mul.64 IR8, IR8, IR9
      mul.64 IR8, IR8, IR1
      @dead IR2, IR3, IR4, IR5, IR6, IR7
      ret.64 IR8
    @end
  @end

  @method test_copy_local_g(I64, I64)I64
    @code
      @live.prim IR1, IR2
      mov.64 IR5, IR1
      mov.64 IR6, IR2
      @dead IR1, IR2
      load.type.info IR10, mem:Rec@rec
      newobj mem:Rec@rec
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR3, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR4, IR1
      @dead IR1
      movi.64 IR7, 0xfeed
      st.raw.64 IR5, IR2, 0x0
      st.raw.64 IR6, IR2, 0x8
      st.raw.64 IR7, IR4, 0x0
      @dead IR2, IR3
      @live.rec IR2, IR3
      copy.local.g IR3, IR2, IR10
      ld.raw.64 IR8, IR3, 0x0
      scc.64 EQ, IR8, IR8, IR5
      ld.raw.64 IR9, IR3, 0x8
      scc.64 EQ, IR9, IR9, IR6
      ld.raw.64 IR1, IR4, 0x0
      scc.64 EQ, IR1, IR1, IR7
      mul.64 IR8, IR8, IR9
      mul.64 IR8, IR8, IR1
      @dead IR2, IR3, IR4, IR5, IR6, IR7
      @dead IR10
      ret.64 IR8
    @end
  @end

  @method test_copy_to_local(I64, I64)I64
    @code
      @live.prim IR1, IR2
      mov.64 IR5, IR1
      mov.64 IR6, IR2
      @dead IR1, IR2
      newobj mem:Rec@rec
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR3, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR4, IR1
      @dead IR1
      movi.64 IR7, 0xfeed
      st.raw.64 IR5, IR2, 0x0
      st.raw.64 IR6, IR2, 0x8
      st.raw.64 IR7, IR4, 0x0
      @dead IR2, IR3
      @live.rec IR2, IR3
      copy.to.local IR3, IRZ, IR2, mem:Rec@rec
      ld.raw.64 IR8, IR3, 0x0
      scc.64 EQ, IR8, IR8, IR5
      ld.raw.64 IR9, IR3, 0x8
      scc.64 EQ, IR9, IR9, IR6
      ld.raw.64 IR1, IR4, 0x0
      scc.64 EQ, IR1, IR1, IR7
      mul.64 IR8, IR8, IR9
      mul.64 IR8, IR8, IR1
      @dead IR2, IR3, IR4, IR5, IR6, IR7
      ret.64 IR8
    @end
  @end

  @method test_copy_to_local_g(I64, I64)I64
    @code
      @live.prim IR1, IR2
      mov.64 IR5, IR1
      mov.64 IR6, IR2
      @dead IR1, IR2
      load.type.info IR10, mem:Rec@rec
      newobj mem:Rec@rec
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR3, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR4, IR1
      @dead IR1
      movi.64 IR7, 0xfeed
      st.raw.64 IR5, IR2, 0x0
      st.raw.64 IR6, IR2, 0x8
      st.raw.64 IR7, IR4, 0x0
      @dead IR2, IR3
      @live.rec IR2, IR3
      copy.to.local.g IR3, IRZ, IR2, IR10
      ld.raw.64 IR8, IR3, 0x0
      scc.64 EQ, IR8, IR8, IR5
      ld.raw.64 IR9, IR3, 0x8
      scc.64 EQ, IR9, IR9, IR6
      ld.raw.64 IR1, IR4, 0x0
      scc.64 EQ, IR1, IR1, IR7
      mul.64 IR8, IR8, IR9
      mul.64 IR8, IR8, IR1
      @dead IR2, IR3, IR4, IR5, IR6, IR7
      @dead IR10
      ret.64 IR8
    @end
  @end

  @method test_copy_from_local(I64, I64)I64
    @code
      @live.prim IR1, IR2
      mov.64 IR5, IR1
      mov.64 IR6, IR2
      @dead IR1, IR2
      newobj mem:Rec@rec
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR3, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR4, IR1
      @dead IR1
      movi.64 IR7, 0xfeed
      st.raw.64 IR5, IR2, 0x0
      st.raw.64 IR6, IR2, 0x8
      st.raw.64 IR7, IR4, 0x0
      @dead IR2, IR3
      @live.rec IR2, IR3
      copy.from.local IRZ, IR3, IR2, mem:Rec@rec
      ld.raw.64 IR8, IR3, 0x0
      scc.64 EQ, IR8, IR8, IR5
      ld.raw.64 IR9, IR3, 0x8
      scc.64 EQ, IR9, IR9, IR6
      ld.raw.64 IR1, IR4, 0x0
      scc.64 EQ, IR1, IR1, IR7
      mul.64 IR8, IR8, IR9
      mul.64 IR8, IR8, IR1
      @dead IR2, IR3, IR4, IR5, IR6, IR7
      ret.64 IR8
    @end
  @end

  @method test_copy_from_local_g(I64, I64)I64
    @code
      @live.prim IR1, IR2
      mov.64 IR5, IR1
      mov.64 IR6, IR2
      @dead IR1, IR2
      load.type.info IR10, mem:Rec@rec
      newobj mem:Rec@rec
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR3, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR4, IR1
      @dead IR1
      movi.64 IR7, 0xfeed
      st.raw.64 IR5, IR2, 0x0
      st.raw.64 IR6, IR2, 0x8
      st.raw.64 IR7, IR4, 0x0
      @dead IR2, IR3
      @live.rec IR2, IR3
      copy.from.local.g IRZ, IR3, IR2, IR10
      ld.raw.64 IR8, IR3, 0x0
      scc.64 EQ, IR8, IR8, IR5
      ld.raw.64 IR9, IR3, 0x8
      scc.64 EQ, IR9, IR9, IR6
      ld.raw.64 IR1, IR4, 0x0
      scc.64 EQ, IR1, IR1, IR7
      mul.64 IR8, IR8, IR9
      mul.64 IR8, IR8, IR1
      @dead IR2, IR3, IR4, IR5, IR6, IR7
      @dead IR10
      ret.64 IR8
    @end
  @end
@end
