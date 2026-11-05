@main_type "default"

@type default
  @method test_arr_ic(I64, I64)I64
    @code
      @live.ref IR1
      @live.prim IR2
      arr.len IR3, IR1
      arr.ic IR2, IR3
      ret.64 IR2
    @end
  @end
@end
