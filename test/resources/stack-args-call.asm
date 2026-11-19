@main_type "default"

@method_ref default.callee = default@ref callee(I64,I64,I64,I64,I64,I64,I64,I64,I64)I64

@type default
  ; Nine int-classified params exceed the IReg head area, so the caller passes
  ; the trailing ones on the stack
  ; Written for x86_64: six-register head area, so args 7..9 are stack
  ; params, and the tail register is IR7.
  @method callee(I64,I64,I64,I64,I64,I64,I64,I64,I64)I64
    @code
      @live.prim IR1, IR2, IR3, IR4, IR5, IR6, IR7
                             ; IR1 -- param 1
      add.64 IR1, IR1, IR2   ; add param 2
      add.64 IR1, IR1, IR3   ; add param 3
      add.64 IR1, IR1, IR4   ; add param 4
      add.64 IR1, IR1, IR5   ; add param 5
      add.64 IR1, IR1, IR6   ; add param 6
      @dead IR2
      ld.raw.64 IR2, IR7, 0  ; load param 7
      add.64 IR1, IR1, IR2
      @dead IR2
      ld.raw.64 IR2, IR7, 8  ; load param 8
      add.64 IR1, IR1, IR2
      @dead IR2
      ld.raw.64 IR2, IR7, 16 ; load param 9
      add.64 IR1, IR1, IR2
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
      movi.64 IR7, 0x7
      movi.64 IR8, 0x8
      movi.64 IR9, 0x9
      call.direct #default.callee, IR1, IR2, IR3, IR4, IR5, IR6, IR7, IR8, IR9
      ret.64 IR1
    @end
  @end
@end