@main_type "default"

@method_ref default.scribble = default@ref scribble(I64, I64, I64)I64

@type default
  @method scribble(I64, I64, I64)I64
    @saved_iregs IR8, IR9
    @code
      @live.prim IR1
      movi.64 IR8, 0x5
      movi.64 IR9, 0x6
      @dead IR8, IR9
      ret.64 IR1
    @end
  @end

  @method main(I64)I64
    @code
      @live.prim IR1
      movi.64 IR8, 0x77
      movi.64 IR9, 0x88
      call.direct #default.scribble, IR1, IRZ, IRZ
      add.64 IR1, IR1, IR8
      add.64 IR1, IR1, IR9
      ret.64 IR1
    @end
  @end
@end
