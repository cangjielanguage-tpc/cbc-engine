@main_type "default"
@field_ref obj_r0 = mem:Obj@ref r0 mem:Obj@ref
@type mem:Rec
  @flags PUBLIC RECORD
  @field f0 I64
    @flags PUBLIC
  @end
@end

@type mem:Obj
  @field r0 mem:Obj@ref
    @flags PUBLIC
  @end
@end

@type default
  @method test_copy(I64, I64)I64
    @code
      @live.prim IR1, IR2
      mov.64 IR5, IR1
      mov.64 IR6, IR2
      @dead IR1, IR2
      newobj mem:Rec@rec
      mov.ref IR2, IR1
      mov.64 IR7, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR3, IR1
      mov.64 IR8, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR4, IR1
      @dead IR1
      st.raw.64 IR5, IR3, 0x0
      st.raw.64 IR6, IR3, 0x8
      movi.64 IR9, 0xfeed
      st.raw.64 IR9, IR4, 0x0
      @dead IR7, IR8
      @live.rec IR7, IR8
      copy IR2, IR7, IR3, IR8, mem:Rec@rec
      ld.raw.64 IR1, IR7, 0x0
      scc.64 EQ, IR1, IR1, IR5
      ld.raw.64 IR10, IR7, 0x8
      scc.64 EQ, IR10, IR10, IR6
      mul.64 IR1, IR1, IR10
      @dead IR10
      ld.raw.64 IR10, IR4, 0x0
      scc.64 EQ, IR10, IR10, IR9
      mul.64 IR1, IR1, IR10
      @dead IR2, IR3, IR4, IR5, IR6, IR7, IR8, IR9, IR10
      ret.64 IR1
    @end
  @end

  @method test_copy_g(I64, I64)I64
    @code
      @live.prim IR1, IR2
      mov.64 IR5, IR1
      mov.64 IR6, IR2
      @dead IR1, IR2
      newobj mem:Rec@rec
      mov.ref IR2, IR1
      mov.64 IR7, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR3, IR1
      mov.64 IR8, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR4, IR1
      @dead IR1
      st.raw.64 IR5, IR3, 0x0
      st.raw.64 IR6, IR3, 0x8
      movi.64 IR9, 0xfeed
      st.raw.64 IR9, IR4, 0x0
      load.type.info IR10, mem:Rec@rec
      @dead IR7, IR8
      @live.rec IR7, IR8
      copy.g IR2, IR7, IR3, IR8, IR10
      @dead IR10
      ld.raw.64 IR1, IR7, 0x0
      scc.64 EQ, IR1, IR1, IR5
      ld.raw.64 IR10, IR7, 0x8
      scc.64 EQ, IR10, IR10, IR6
      mul.64 IR1, IR1, IR10
      @dead IR10
      ld.raw.64 IR10, IR4, 0x0
      scc.64 EQ, IR10, IR10, IR9
      mul.64 IR1, IR1, IR10
      @dead IR2, IR3, IR4, IR5, IR6, IR7, IR8, IR9, IR10
      ret.64 IR1
    @end
  @end
@end
