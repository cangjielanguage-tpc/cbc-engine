@main_type "default"

@type default
  @method test_FNGE(F32, F32)I64
    @code
      @live.prim IR1
      fscc.32 FNGE, IR1, FR0, FR1
      ret.64 IR1
    @end
  @end
@end
