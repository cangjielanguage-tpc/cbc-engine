@main_type "default"

@type default
  @method main()I32
    @code
      movf2i.32 IR1, FR0
      ret.32 IR1
    @end
  @end
@end
