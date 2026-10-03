;strict

; Disasm-only
;@aotdeps cangjie-std-core

@main_type "default"

@type std.core:Object
  @flags PUBLIC AOT
@end

@type default:Foo
  @flags PUBLIC

  @super std.core:Object@aref

  @method dummy()Void
    @flags VIRTUAL
    @code
      movi.64 IR1, 0x123
      ret.64 IR1
    @end
  @end

  @method foo()Void
    @flags VIRTUAL REF_RECEIVER
    @code
      movi.64 IR1, 0x123
      ret.64 IR1
    @end
  @end
@end

@method_ref foo = default:Foo@ref foo()Void [REF_RECEIVER]

@type default

  @method main()I64
    @code
      newobj default:Foo@ref
      call.virt #foo, IR1
      @dead IR1
      @live.prim IR1
      ret.64 IR1
    @end
  @end
@end
