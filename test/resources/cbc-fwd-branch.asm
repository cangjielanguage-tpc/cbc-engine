;strict
@main_type "default"

@type default
  @method main()I64
    @code
      movi.64 IR3, 0x0
      bcc.64 EQ, IR3, IRZ, r
      movi.64 IR1, 0x1
r:
      ret.64 IR1
    @end
  @end
@end
