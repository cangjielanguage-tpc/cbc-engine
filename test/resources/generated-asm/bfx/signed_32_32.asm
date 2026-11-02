@main_type "default"

@type default
  @method test_signed_32_32(I64)I64
    @code
      @live.prim IR1
      bfxs.32.32 IR1, IR1, 0, 32
      ret.64 IR1
    @end
  @end
@end
