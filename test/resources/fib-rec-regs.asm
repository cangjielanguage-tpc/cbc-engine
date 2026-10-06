;strict
@main_type "default"

@method_ref default.fib = default@ref fib(I64)I64

@type default

  @method main()I64
    @code
      movi.64 IR1, 0x7
      call.direct #default.fib, IR1
      ret.64 IR1
    @end
  @end

  @method fib(I64)I64
    @untyped_count 0x3

    @code
      @live.prim IR1
      bcci.64 LE, IR1, 0x1, r
      mov.64 IR8, IR1
      subi.64 IR1, IR1, 0x1
      call.direct #default.fib, IR1
      mov.64 IR9, IR1
      @dead IR1
      subi.64 IR1, IR8, 0x2
      @dead IR8
      call.direct #default.fib, IR1
      add.64 IR1, IR1, IR9
      @dead IR9
r:
      ret.64 IR1
    @end
  @end
@end
