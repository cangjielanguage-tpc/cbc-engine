@main_type "default"
@type mem:Opt1
  @flags PUBLIC
  @enum_kind Option1
  @enum I64
@end

@type mem:Opt0
  @flags PUBLIC
  @enum_kind Option0
  @enum I64
@end

@type default
  @method test_tag_opt1(I64)I64
    @code
      @live.prim IR1
      mov.64 IR2, IR1
      @dead IR1
      newobj mem:Opt1@ref
      st.raw.64 IR2, IR1, 0x8
      load.type.info IR3, I64
      tag.g IR4, IR1, IR3, mem:Opt1@ref
      @dead IR1, IR2, IR3
      ret.64 IR4
    @end
  @end

  @method test_tag_opt0(I64)I64
    @code
      @live.prim IR1
      mov.64 IR2, IR1
      @dead IR1
      newobj mem:Opt0@ref
      st.raw.64 IR2, IR1, 0x8
      load.type.info IR3, I64
      tag.g IR4, IR1, IR3, mem:Opt0@ref
      @dead IR1, IR2, IR3
      ret.64 IR4
    @end
  @end
@end
