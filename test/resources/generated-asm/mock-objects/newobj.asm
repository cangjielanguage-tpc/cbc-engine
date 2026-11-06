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
  @method test_ptr()I64
    @code
      newobj mem:Obj@ref
      ret.ref IR1
    @end
  @end

  @method test_field()I64
    @code
      @live.prim IR2
      newobj mem:Obj@ref
      st.field IR2, IR1, #obj_x
      @dead IR2
      ret.ref IR1
    @end
  @end

  @method test_distinct()I64
    @code
      newobj mem:Rec@rec
      mov.ref IR4, IR1
      @dead IR1
      newobj mem:Rec@rec
      movi.64 IR3, 0x1
      bcc.64 RNE, IR1, IR4, skip
      subi.64 IR3, IR3, 0x1
      skip:
      @dead IR1, IR4
      ret.64 IR3
    @end
  @end

  @method test_ti()I64
    @code
      newobj mem:Rec@rec
      load.type.info IR2, mem:Rec@rec
      ld.raw.64 IR3, IR1, 0
      scc.64 EQ, IR4, IR2, IR3
      @dead IR1, IR2, IR3
      ret.64 IR4
    @end
  @end

  @method test_g()I64
    @code
      load.type.info IR4, mem:Rec@rec
      newobj.g IR4, mem:Rec@rec
      ld.raw.64 IR3, IR1, 0
      scc.64 EQ, IR5, IR4, IR3
      @dead IR1, IR3, IR4
      ret.64 IR5
    @end
  @end
@end
