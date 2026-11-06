@main_type "default"
@type mem:Intf
  @flags PUBLIC INTERFACE
  @method foo(I64)I64
    @flags PUBLIC ABSTRACT VIRTUAL
  @end
@end

@type mem:Impl
  @interfaces mem:Intf@ref
  @method impl_foo(I64)I64
    @flags PUBLIC VIRTUAL
    @code
      @live.prim IR1
      load.type.info IR3, mem:Impl@ref
      ld.raw.64 IR4, IR1, 0
      scc.64 EQ, IR4, IR4, IR3
      ld.raw.64 IR2, IR1, 8
      addi.64 IR2, IR2, 0x1
      mul.64 IR2, IR2, IR4
      @dead IR1, IR3, IR4
      mov.64 IR1, IR2
      @dead IR2
      ret.64 IR1
    @end
  @end
@end

@method_ref intf_foo = mem:Intf@ref foo(I64)I64

@type default
  @method main(I64)I64
    @code
      @live.prim IR1
      mov.64 IR2, IR1
      @dead IR1
      newobj mem:Impl@ref
      st.raw.64 IR2, IR1, 0x8
      call.interf #intf_foo, IR1
      @dead IR1
      @live.prim IR1
      ret.64 IR1
    @end
  @end
@end
