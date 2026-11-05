@main_type "default"

@method_ref default.sum6i8f = default@ref sum6i8f(I64, I64, I64, I64, I64, I64, F64, F64, F64, F64, F64, F64, F64, F64)I64

@type default
  @method sum6i8f(I64, I64, I64, I64, I64, I64, F64, F64, F64, F64, F64, F64, F64, F64)I64
    @code
      @live.prim IR1, IR2, IR3, IR4, IR5, IR6
      fadd.64 FR0, FR0, FR1
      fadd.64 FR0, FR0, FR2
      fadd.64 FR0, FR0, FR3
      fadd.64 FR0, FR0, FR4
      fadd.64 FR0, FR0, FR5
      fadd.64 FR0, FR0, FR6
      fadd.64 FR0, FR0, FR7
      add.64 IR1, IR1, IR2
      add.64 IR1, IR1, IR3
      add.64 IR1, IR1, IR4
      add.64 IR1, IR1, IR5
      add.64 IR1, IR1, IR6
      @dead IR2, IR3, IR4, IR5, IR6
      f2i F64, I64, FR0, IR2
      add.64 IR1, IR1, IR2
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
      fmovi.64 FR0, 1.0
      fmovi.64 FR1, 2.0
      fmovi.64 FR2, 3.0
      fmovi.64 FR3, 4.0
      fmovi.64 FR4, 5.0
      fmovi.64 FR5, 6.0
      fmovi.64 FR6, 7.0
      fmovi.64 FR7, 8.0
      call.direct #default.sum6i8f, IR1, IR2, IR3, IR4, IR5, IR6, FR0, FR1, FR2, FR3, FR4, FR5, FR6, FR7
      ret.64 IR1
    @end
  @end
@end
