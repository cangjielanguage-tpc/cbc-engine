@main_type "default"

@type default
  @method test_16(I64)I64
    @code
      @live.prim IR1
      cuaddi.16 IR1, IR1, 0x1000
      ret.64 IR1
    @end
  @end

  @method test_32(I64)I64
    @code
      @live.prim IR1
      cuaddi.32 IR1, IR1, 0x1000
      ret.64 IR1
    @end
  @end

  @method test_64(I64)I64
    @code
      @live.prim IR1
      cuaddi.64 IR1, IR1, 0x1000
      ret.64 IR1
    @end
  @end
@end
