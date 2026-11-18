@main_type "default"

@method_ref default.callee = default@ref callee(I64,I64,I64,I64,I64,I64,I64,I64,I64)I64

@type default
  ; Nine int-classified params exceed the IReg head area of every supported
  ; platform, so the caller passes the trailing arguments on the stack. The
  ; rewriter stores them through st.stack.param*, relative to the caller's
  ; frame bot.
  @method callee(I64,I64,I64,I64,I64,I64,I64,I64,I64)I64
    @code
      @live.prim IR1, IR2, IR3, IR4, IR5, IR6
      add.64 IR1, IR1, IR2
      add.64 IR1, IR1, IR3
      add.64 IR1, IR1, IR4
      add.64 IR1, IR1, IR5
      add.64 IR1, IR1, IR6
      ret.64 IR1
    @end
  @end

  @method main()I64
    @code
      movi.64 IR1, 0x1
      movi.64 IR2, 0x2
      movi.64 IR3, 0x3
      movi.64 IR4, 0x4
      movi.64 IR5, 0x5
      movi.64 IR6, 0x6
      call.direct #default.callee, IR1, IR2, IR3, IR4, IR5, IR6, IR4, IR5, IR6
      ret.64 IR1
    @end
  @end
@end