@main_type "default"

@type default
  @method test_loop_jmp(I64)I64
    @code
      @live.prim IR1
      movi.64 IR2, 0x0
      loop:
      add.64 IR2, IR2, IR1
      subi.64 IR1, IR1, 0x1
      bcc.64 EQ, IR1, IRZ, done
      jmp loop
      done:
      ret.64 IR2
    @end
  @end
@end
