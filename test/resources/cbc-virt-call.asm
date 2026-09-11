;strict

; Disasm-only
;@aotdeps cangjie-std-core

@main_type "default"

@type std.core:Object
  @flags PUBLIC AOT
@end

@type Foo
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
    @flags VIRTUAL
    @code
      movi.64 IR1, 0x123
      ret.64 IR1
    @end
  @end
@end

@method_ref Foo.foo = Foo@ref foo()Void

@type default

  @method cj_entry()I64
    @code
      call.virt IR1, #Foo.foo
      @live.prim IR1
      ret.64 IR1
    @end
  @end
@end
