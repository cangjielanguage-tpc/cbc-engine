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
  @method test_payload_raw_opt1(I64)I64
    @code
      @live.prim IR1
      mov.64 IR2, IR1
      @dead IR1
      newobj mem:Opt1@ref
      st.raw.64 IR2, IR1, 0x8
      load.type.info IR3, I64
      load.type.info IR4, mem:Opt1@ref
      payload.g IR5, IR1, IR3, IR4, mem:Opt1@ref
      @dead IR1, IR2, IR3, IR4
      ret.ref IR5
    @end
  @end

  @method test_payload_raw_opt0(I64)I64
    @code
      @live.prim IR1
      mov.64 IR2, IR1
      @dead IR1
      newobj mem:Opt0@ref
      st.raw.64 IR2, IR1, 0x8
      load.type.info IR3, I64
      load.type.info IR4, mem:Opt0@ref
      payload.g IR5, IR1, IR3, IR4, mem:Opt0@ref
      @dead IR1, IR2, IR3, IR4
      ret.ref IR5
    @end
  @end

  @method test_payload_gen_opt1(I64)I64
    @code
      @live.ref IR1
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Opt1@ref
      lea.box IR3, IR1
      load.type.info IR4, I64
      st.g IR2, IR1, IR3, IR4
      load.type.info IR5, mem:Opt1@ref
      payload.g IR6, IR1, IR4, IR5, mem:Opt1@ref
      @dead IR1, IR2, IR3, IR4, IR5
      ret.ref IR6
    @end
  @end

  @method test_payload_gen_opt0(I64)I64
    @code
      @live.ref IR1
      mov.ref IR2, IR1
      @dead IR1
      newobj mem:Opt0@ref
      lea.box IR3, IR1
      load.type.info IR4, I64
      st.g IR2, IR1, IR3, IR4
      load.type.info IR5, mem:Opt0@ref
      payload.g IR6, IR1, IR4, IR5, mem:Opt0@ref
      @dead IR1, IR2, IR3, IR4, IR5
      ret.ref IR6
    @end
  @end
@end
