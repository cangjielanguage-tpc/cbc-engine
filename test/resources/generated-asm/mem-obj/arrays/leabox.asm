@main_type "default"

@type default
  @method test_leabox(I64)I64
    @code
      @live.ref IR1
      lea.box IR2, IR1
      @dead IR1
      ret.64 IR2
    @end
  @end
@end
