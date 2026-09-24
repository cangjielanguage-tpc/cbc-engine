;strict
; Resolution only: no static call in this file is executed.
@main_type "default"

@type Operation
  @flags PUBLIC INTERFACE
  @type_vars 1

  @method calculate(%0) %0
    @flags PUBLIC STATIC VIRTUAL ABSTRACT SRET HAS_THIS_TI HAS_OUTER_TI
  @end
@end

; The interface's %0 is the caller's %1, or a concrete Float64.
; Both calls still pass the declared argument through an integer register.
@method_ref generic = Operation[%1]@ref calculate(%0) %0 [SRET, HAS_THIS_TI, HAS_OUTER_TI]
@method_ref concrete = Operation[F64]@ref calculate(%0) %0 [SRET, HAS_THIS_TI, HAS_OUTER_TI]

@type default
  @method main() I64
    @code
      call.interf IR1, #generic
      call.interf IR1, #concrete
      @live.prim IR1
      ret.64 IR1
    @end
  @end
@end
