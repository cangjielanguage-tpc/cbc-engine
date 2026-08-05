import trampoline.goto_trampoline

public func cj_entry(): Int64 {
    try {
        return goto_trampoline() // should call c-function from trampoline.c
    } catch (e: OutOfMemoryError) {
        println("An exception has occured:    Out of memory")
        return -1
    } catch (e: Error) {
        e.printStackTrace()
        return -1
    } catch (e: Exception) {
        // FIXME: require package-private std.core.handleException function
        e.printStackTrace()
        return -1
    }
}
