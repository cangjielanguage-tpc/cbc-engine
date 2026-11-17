@main_type "default"

@method_ref default.sizeGSpilled = default@ref sizeGSpilled(I64,I64,I64,I64,I64,I64,I64)I64 (%0)
@method_ref default.alignGSpilled = default@ref alignGSpilled(I64,I64,I64,I64,I64,I64,I64)I64 (%0)

@type default
  ; 7 int-classified signature params push the incoming GenericFuncParams
  ; TypeInfo past the IReg head area (6 registers on x64): the TI arrives in
  ; a caller stack slot and size.g/align.g must reload it via the tail register.
  ; main receives the TypeInfo pointer in IR1 from the test harness.
  @method sizeGSpilled(I64,I64,I64,I64,I64,I64,I64)I64
    @generic_params 1
    @code
      size.g IR1, 0
      ret.64 IR1
    @end
  @end

  @method alignGSpilled(I64,I64,I64,I64,I64,I64,I64)I64
    @generic_params 1
    @code
      align.g IR1, 0
      ret.64 IR1
    @end
  @end

  @method main()I64
    @code
      @live.prim IR1
      call.direct #default.sizeGSpilled, IRZ, IRZ, IRZ, IRZ, IRZ, IRZ, IRZ, IR1
      ret.64 IR1
    @end
  @end
@end
