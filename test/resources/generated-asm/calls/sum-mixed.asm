@main_type "default"

@method_ref default.sum_mixed = default@ref sum_mixed(I64, I64, I64, I64, I64, I64, F64, F64, F64, F64)I64

@type default
  @method sum_mixed(I64, I64, I64, I64, I64, I64, F64, F64, F64, F64)I64
    @code
      @live.prim IR1, IR2, IR3, IR4, IR5, IR6
      add.64 IR1, IR1, IR2
      add.64 IR1, IR1, IR3
      add.64 IR1, IR1, IR4
      add.64 IR1, IR1, IR5
      add.64 IR1, IR1, IR6
      @dead IR2, IR3, IR4, IR5, IR6
      f2i F64, I64, FR0, IR2
      f2i F64, I64, FR1, IR3
      f2i F64, I64, FR2, IR4
      f2i F64, I64, FR3, IR5
      add.64 IR1, IR1, IR2
      add.64 IR1, IR1, IR3
      add.64 IR1, IR1, IR4
      add.64 IR1, IR1, IR5
      ret.64 IR1
    @end
  @end

  @method main(I64)I64
    @code
      @live.prim IR1
      addi.64 IR2, IR1, 0x1
      addi.64 IR3, IR1, 0x2
      addi.64 IR4, IR1, 0x3
      addi.64 IR5, IR1, 0x4
      addi.64 IR6, IR1, 0x5
      fmovi.64 FR0, 10.0
      fmovi.64 FR1, 20.0
      fmovi.64 FR2, 30.0
      fmovi.64 FR3, 40.0
      call.direct #default.sum_mixed, IR1, IR2, IR3, IR4, IR5, IR6, FR0, FR1, FR2, FR3
      ret.64 IR1
    @end
  @end
@end
