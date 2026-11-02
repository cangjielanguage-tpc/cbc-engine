@main_type "default"

@type default
  @method test_F2I(F32)I32
    @code
      movf2i.32 IR1, FR0
      ret.32 IR1
    @end
  @end
@end
