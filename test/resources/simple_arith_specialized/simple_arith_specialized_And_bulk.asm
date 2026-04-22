@type default {
  @method default test_0x1000020 [ ] I64 {
    live.prim [ IR1 ]
    andi.64 IR1, IR1, 0x1000020
    ret.64 IR1
  }

  @method default test_0x10000 [ ] I64 {
    live.prim [ IR1 ]
    andi.64 IR1, IR1, 0x10000
    ret.64 IR1
  }

  @method default test_0x100200 [ ] I64 {
    live.prim [ IR1 ]
    andi.64 IR1, IR1, 0x100200
    ret.64 IR1
  }

  @method default test_0x100 [ ] I64 {
    live.prim [ IR1 ]
    andi.64 IR1, IR1, 0x100
    ret.64 IR1
  }

  @method default test_0x1020 [ ] I64 {
    live.prim [ IR1 ]
    andi.64 IR1, IR1, 0x1020
    ret.64 IR1
  }

  @method default test_0x10 [ ] I64 {
    live.prim [ IR1 ]
    andi.64 IR1, IR1, 0x10
    ret.64 IR1
  }

  @method default test_0x1 [ ] I64 {
    live.prim [ IR1 ]
    andi.64 IR1, IR1, 0x1
    ret.64 IR1
  }

  @method default test_0x7000000000000000 [ ] I64 {
    live.prim [ IR1 ]
    andi.64 IR1, IR1, 0x7000000000000000
    ret.64 IR1
  }

  @method default test_0x7000000010000001 [ ] I64 {
    live.prim [ IR1 ]
    andi.64 IR1, IR1, 0x7000000010000001
    ret.64 IR1
  }

  @method default test_0xf000000100000001 [ ] I64 {
    live.prim [ IR1 ]
    andi.64 IR1, IR1, 0xf000000100000001
    ret.64 IR1
  }

  @method default test_0xf000100000000001 [ ] I64 {
    live.prim [ IR1 ]
    andi.64 IR1, IR1, 0xf000100000000001
    ret.64 IR1
  }

  @method default test_0xffffffffffff1121 [ ] I64 {
    live.prim [ IR1 ]
    andi.64 IR1, IR1, 0xffffffffffff1121
    ret.64 IR1
  }
}
