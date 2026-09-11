;strict
@main_type "default"

@type default
  @method cj_entry()I64
    @code
      @live.prim IR1
      bfxz.32.64 IR1, IR1, 0, 32
      ret.64 IR1
    @end
  @end
@end
