@main_type "default"

; generic record without fields, used to nest a function type variable
@type default:Foo
  @flags PUBLIC RECORD
  @type_vars 1
@end

@type default
  ; open terms the runtime hole binding cannot serve: function type variables
  ; are never substituted at runtime and terms with more than 6 subterms
  ; exceed the registers the runtime reads (IR1+i)
  @method sizeFuncTv()I64
    @generic_params 1
    @code
      size IR2, %%0
      ret.64 IR2
    @end
  @end

  @method sizeNestedFuncTv()I64
    @generic_params 1
    @code
      size IR2, default:Foo[%%0]@rec
      ret.64 IR2
    @end
  @end

  @method sizeTooManySubterms()I64
    @generic_params 1
    @code
      size IR2, [I32, I32, I32, I32, I32, I32, %0]
      ret.64 IR2
    @end
  @end
@end
