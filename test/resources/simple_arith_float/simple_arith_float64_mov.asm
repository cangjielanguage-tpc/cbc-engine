@main_type default

@type default {
  @method default test_MOV [ ] F64 {
    mov.64 FR0, FR1
    fret.64 FR0
  }

  @method default test_I2F [ ] F64 {
    live.prim [ IR1 ]
    mov.64 FR0, IR1
    fret.64 FR0
  }

  @method default test_F2I [ ] I64 {
    mov.64 IR1, FR0
    ret.64 IR1
  }
}
