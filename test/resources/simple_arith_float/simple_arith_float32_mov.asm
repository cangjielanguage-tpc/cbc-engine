@type default {
  @method default test_MOV [ ] F32 {
    mov.32 FR0, FR1
    fret.32 FR0
  }

  @method default test_I2F [ ] F32 {
    live.prim [ IR1 ]
    mov.32 FR0, IR1
    fret.32 FR0
  }

  @method default test_F2I [ ] I32 {
    mov.32 IR1, FR0
    ret.32 IR1
  }
}
