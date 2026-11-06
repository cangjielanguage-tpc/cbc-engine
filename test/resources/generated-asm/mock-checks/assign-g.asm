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
  @method test_assign_g(I64)I64
    @code
      @live.prim IR1
      mov.64 IR2, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR3, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR4, IR1
      @dead IR1
      movi.64 IR5, 0x1234567890abcdef
      st.raw.64 IR5, IR3, 0x0
      st.raw.64 IR5, IR3, 0x8
      st.raw.64 IR2, IR4, 0x8
      load.type.info IR6, mem:Rec@rec
      assign.g IR3, IR4, IR6
      ld.raw.64 IR7, IR3, 0x0
      scc.64 EQ, IR7, IR7, IR5
      ld.raw.64 IR8, IR3, 0x8
      scc.64 EQ, IR8, IR8, IR2
      ld.raw.64 IR9, IR4, 0x0
      scc.64 EQ, IR9, IR9, IR6
      ld.raw.64 IR10, IR4, 0x8
      scc.64 EQ, IR10, IR10, IR2
      mul.64 IR7, IR7, IR8
      mul.64 IR7, IR7, IR9
      mul.64 IR7, IR7, IR10
      @dead IR2, IR3, IR4, IR5, IR6, IR8, IR9, IR10
      ret.64 IR7
    @end
  @end

  @method test_assign_g_dst(I64)I64
    @code
      @live.prim IR1
      mov.64 IR2, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR3, IR1
      @dead IR1
      newobj mem:Rec@rec
      mov.ref IR4, IR1
      @dead IR1
      st.raw.64 IR2, IR4, 0x8
      load.type.info IR5, mem:Rec@rec
      assign.g IR3, IR4, IR5
      @dead IR2, IR4, IR5
      ret.ref IR3
    @end
  @end
@end
