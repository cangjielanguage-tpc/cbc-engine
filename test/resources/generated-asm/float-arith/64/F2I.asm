@main_type "default"

@type default
  @method test_F2I(F64)I64
    @code
      movf2i.64 IR1, FR0
      ret.64 IR1
    @end
  @end
@end
