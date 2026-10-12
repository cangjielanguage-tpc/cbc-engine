"""State: cache cleared on inferior exit."""

from api import NewLibraryEvent, debugger
from functools import wraps
from typing import Any, Callable

_cache: dict = {}


def _clear_cache(_event: Any = None) -> None:
    print("[CBC-plugin] Cache cleared!")
    _cache.clear()

def cached(func: Callable) -> Callable:
    """Decorator that caches results by (func_name, args, kwargs)."""
    @wraps(func)
    def wrapper(*args: Any, **kwargs: Any) -> Any:
        key = (func.__name__, args, tuple(sorted(kwargs.items())))
        if key not in _cache:
            _cache[key] = func(*args, **kwargs)
        return _cache[key]
    return wrapper

def _lib_start_debug(event: NewLibraryEvent):
    if not event.filename:
        return
    name = event.filename
    if "libcbcengine.so" not in name:
        return
    sym = event.symbol("engine_store_debug_names")
    if sym is None:
        print("[CBC-plugin] WARNING: symbol 'engine_store_debug_names' not found")
        return
    sym.set(1)
    print(f"[CBC-plugin] engine_store_debug_names set to 1 @ 0x{sym.location:x}")


debugger().on_exit(_clear_cache)
debugger().on_lib_load(_lib_start_debug)
