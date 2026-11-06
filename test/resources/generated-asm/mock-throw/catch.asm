@main_type "default"
@type mem:Exc
  @field x I64
    @flags PUBLIC
  @end
@end

@type default
  @method test_catch(I64)I64
    @code
      @live.prim IR1
      mov.64 IR2, IR1
      @dead IR1
      newobj mem:Exc@ref
      mov.ref IR3, IR1
      st.raw.64 IR2, IR1, 0x8
      throw IR1
      @dead IR1, IR3
      catch IR1
      @live.ref IR3
      movi.64 IR6, 0x1
      ld.raw.64 IR5, IR1, 0x8
      scc.64 EQ, IR7, IR5, IR2
      mul.64 IR6, IR6, IR7
      bcc.64 REQ, IR1, IR3, ok
      subi.64 IR6, IR6, 0x1
      ok:
      @dead IR1, IR2, IR3, IR5, IR7
      ret.64 IR6
    @end
  @end

  @method test_caught_ptr(I64)I64
    @code
      @live.prim IR1
      mov.64 IR2, IR1
      @dead IR1
      newobj mem:Exc@ref
      st.raw.64 IR2, IR1, 0x8
      throw IR1
      @dead IR1, IR2
      catch IR1
      ret.ref IR1
    @end
  @end

  @method test_throw_param(I64)I64
    @code
      @live.ref IR1
      mov.ref IR3, IR1
      movi.64 IR6, 0x1
      throw IR1
      @dead IR1, IR3
      catch IR1
      @live.ref IR3
      bcc.64 REQ, IR1, IR3, ok
      subi.64 IR6, IR6, 0x1
      ok:
      @dead IR1, IR3
      ret.64 IR6
    @end
  @end
@end
