import trampoline.goto_trampoline

public func cj_entry(): Int64 {
    try {
        return goto_trampoline() // should call c-function from trampoline.c
    } catch (e: Exception) {
        e.printStackTrace()
	return -1
    }
}