@main_type "default"

@type default
  @method test_FEQ(F64, F64)I64
    @code
      @live.prim IR1
      fscc.64 FEQ, IR1, FR0, FR1
      ret.64 IR1
    @end
  @end
@end
