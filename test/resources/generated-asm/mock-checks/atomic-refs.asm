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
  @method test_atomic_st_ld()I64
    @code
      newobj mem:Obj@ref
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Obj@ref
      mov.ref IR3, IR1
      @dead IR1
      atomic.store IR3, IR2, #obj_r0
      atomic.load IR4, IR2, #obj_r0
      movi.64 IR5, 0x1
      bcc.64 REQ, IR4, IR3, ok
      subi.64 IR5, IR5, 0x1
      ok:
      @dead IR2, IR3, IR4
      ret.64 IR5
    @end
  @end

  @method test_atomic_st_ld_ptr()I64
    @code
      newobj mem:Obj@ref
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Obj@ref
      mov.ref IR3, IR1
      @dead IR1
      atomic.store IR3, IR2, #obj_r0
      atomic.load IR4, IR2, #obj_r0
      @dead IR2, IR3
      ret.ref IR4
    @end
  @end

  @method test_atomic_st_ptr()I64
    @code
      newobj mem:Obj@ref
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Obj@ref
      mov.ref IR3, IR1
      @dead IR1
      atomic.store IR3, IR2, #obj_r0
      @dead IR3
      ret.ref IR2
    @end
  @end

  @method test_atomic_val_ptr()I64
    @code
      newobj mem:Obj@ref
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Obj@ref
      mov.ref IR3, IR1
      @dead IR1
      @dead IR2
      ret.ref IR3
    @end
  @end

  @method test_atomic_swap()I64
    @code
      newobj mem:Obj@ref
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Obj@ref
      mov.ref IR3, IR1
      @dead IR1
      newobj mem:Obj@ref
      mov.ref IR4, IR1
      @dead IR1
      st.field IR3, IR2, #obj_r0
      atomic.swap IR5, IR2, IR4, #obj_r0
      movi.64 IR6, 0x1
      bcc.64 REQ, IR5, IR3, ok1
      subi.64 IR6, IR6, 0x1
      ok1:
      atomic.load IR7, IR2, #obj_r0
      bcc.64 REQ, IR7, IR4, ok2
      subi.64 IR6, IR6, 0x1
      ok2:
      @dead IR2, IR3, IR4, IR5, IR7
      ret.64 IR6
    @end
  @end

  @method test_atomic_swap_old()I64
    @code
      newobj mem:Obj@ref
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Obj@ref
      mov.ref IR3, IR1
      @dead IR1
      newobj mem:Obj@ref
      mov.ref IR4, IR1
      @dead IR1
      st.field IR3, IR2, #obj_r0
      atomic.swap IR5, IR2, IR4, #obj_r0
      @dead IR2, IR3, IR4
      ret.ref IR5
    @end
  @end

  @method test_atomic_cas_ok()I64
    @code
      newobj mem:Obj@ref
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Obj@ref
      mov.ref IR3, IR1
      @dead IR1
      newobj mem:Obj@ref
      mov.ref IR4, IR1
      @dead IR1
      st.field IR3, IR2, #obj_r0
      atomic.cas IR5, IR2, IR3, IR4, #obj_r0
      movi.64 IR6, 0x1
      movi.64 IR7, 0x1
      bcc.64 EQ, IR5, IR7, ok1
      subi.64 IR6, IR6, 0x1
      ok1:
      atomic.load IR8, IR2, #obj_r0
      bcc.64 REQ, IR8, IR4, ok2
      subi.64 IR6, IR6, 0x1
      ok2:
      @dead IR2, IR3, IR4, IR5, IR7, IR8
      ret.64 IR6
    @end
  @end

  @method test_atomic_cas_fail()I64
    @code
      newobj mem:Obj@ref
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Obj@ref
      mov.ref IR3, IR1
      @dead IR1
      newobj mem:Obj@ref
      mov.ref IR4, IR1
      @dead IR1
      st.field IR3, IR2, #obj_r0
      atomic.cas IR5, IR2, IR4, IR3, #obj_r0
      movi.64 IR6, 0x1
      movi.64 IR7, 0x0
      bcc.64 EQ, IR5, IR7, ok1
      subi.64 IR6, IR6, 0x1
      ok1:
      atomic.load IR8, IR2, #obj_r0
      bcc.64 REQ, IR8, IR3, ok2
      subi.64 IR6, IR6, 0x1
      ok2:
      @dead IR2, IR3, IR4, IR5, IR7, IR8
      ret.64 IR6
    @end
  @end
@end
