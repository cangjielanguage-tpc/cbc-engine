@main_type "default"

@method_ref default.leaf = default@ref leaf(I64)I64
@method_ref default.mid = default@ref mid(I64, I64)I64

@type default
  @method leaf(I64)I64
    @code
      @live.prim IR1
      add.64 IR1, IR1, IR1
      ret.64 IR1
    @end
  @end

  @method mid(I64, I64)I64
    @untyped_count 0x2
    @code
      @live.prim IR1
      st.uslot.64 IR1, $0
      subi.64 IR2, IR1, 0x1
      call.direct #default.leaf, IR2
      @dead IR2
      ld.uslot.64 IR2, $0
      ld.uslot.64 IR3, $0
      add.64 IR1, IR1, IR2
      add.64 IR1, IR1, IR3
      @dead IR2, IR3, $0
      ret.64 IR1
    @end
  @end

  @method main(I64)I64
    @untyped_count 0x1
    @code
      @live.prim IR1
      st.uslot.64 IR1, $0
      call.direct #default.mid, $0, IRZ
      ret.64 IR1
    @end
  @end
@end
