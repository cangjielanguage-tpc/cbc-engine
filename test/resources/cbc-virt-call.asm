@type Foo {
  @typeflags PUBLIC

  @method default dummy [ ] Void {
    @methodflags VIRTUAL
    mov.64 IR1, 0x123
    ret.64 IR1
  }

  @method default foo [ ] Void {
    @methodflags VIRTUAL
    mov.64 IR1, 0x123
    ret.64 IR1
  }
}

@type default {
  @methodref Foo.foo, VIRTUAL Foo foo [ ] Void

  @method default main [ ] I64 {
    call.virt Method(Foo.foo), IR1
    live.prim [ IR1 ]
    ret.64 IR1
  }
}
