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
  @method test_none_opt0()I64
    @code
      load.type.info IR3, I64
      load.type.info IR4, mem:Opt0@ref
      new.none.g IR1, IR3, IR4, mem:Opt0@ref
      @dead IR3, IR4
      ret.ref IR1
    @end
  @end

  @method test_none_opt1()I64
    @code
      load.type.info IR3, I64
      load.type.info IR4, mem:Opt1@ref
      new.none.g IR1, IR3, IR4, mem:Opt1@ref
      @dead IR3, IR4
      ret.ref IR1
    @end
  @end

  @method test_none_tag_opt0()I64
    @code
      load.type.info IR3, I64
      load.type.info IR4, mem:Opt0@ref
      new.none.g IR1, IR3, IR4, mem:Opt0@ref
      tag.g IR5, IR1, IR3, mem:Opt0@ref
      @dead IR1, IR3, IR4
      ret.64 IR5
    @end
  @end

  @method test_none_tag_opt1()I64
    @code
      load.type.info IR3, I64
      load.type.info IR4, mem:Opt1@ref
      new.none.g IR1, IR3, IR4, mem:Opt1@ref
      tag.g IR5, IR1, IR3, mem:Opt1@ref
      @dead IR1, IR3, IR4
      ret.64 IR5
    @end
  @end

  @method test_some_opt0(I64)I64
    @code
      @live.prim IR1
      mov.64 IR2, IR1
      @dead IR1
      load.type.info IR3, I64
      load.type.info IR4, mem:Opt0@ref
      new.some.g IR1, IR2, IR3, IR4, mem:Opt0@ref
      @dead IR2, IR3, IR4
      ret.ref IR1
    @end
  @end

  @method test_some_opt1(I64)I64
    @code
      @live.prim IR1
      mov.64 IR2, IR1
      @dead IR1
      load.type.info IR3, I64
      load.type.info IR4, mem:Opt1@ref
      new.some.g IR1, IR2, IR3, IR4, mem:Opt1@ref
      @dead IR2, IR3, IR4
      ret.ref IR1
    @end
  @end

  @method test_some_payload_opt0(I64)I64
    @code
      @live.prim IR1
      mov.64 IR2, IR1
      @dead IR1
      load.type.info IR3, I64
      load.type.info IR4, mem:Opt0@ref
      new.some.g IR1, IR2, IR3, IR4, mem:Opt0@ref
      @dead IR2
      payload.g IR5, IR1, IR3, IR4, mem:Opt0@ref
      @dead IR1, IR3, IR4
      ret.ref IR5
    @end
  @end

  @method test_some_payload_opt1(I64)I64
    @code
      @live.prim IR1
      mov.64 IR2, IR1
      @dead IR1
      load.type.info IR3, I64
      load.type.info IR4, mem:Opt1@ref
      new.some.g IR1, IR2, IR3, IR4, mem:Opt1@ref
      @dead IR2
      payload.g IR5, IR1, IR3, IR4, mem:Opt1@ref
      @dead IR1, IR3, IR4
      ret.ref IR5
    @end
  @end
@end
