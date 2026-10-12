"""State: cache cleared on inferior exit."""

from api import debugger
from functools import wraps
from typing import Any, Callable

_cache: dict = {}


def _clear_cache(_event: Any = None) -> None:
    print("[CBC-plugin] Cache cleared!")
    _cache.clear()

debugger().on_exit(_clear_cache)

def cached(func: Callable) -> Callable:
    """Decorator that caches results by (func_name, args, kwargs)."""
    @wraps(func)
    def wrapper(*args: Any, **kwargs: Any) -> Any:
        key = (func.__name__, args, tuple(sorted(kwargs.items())))
        if key not in _cache:
            _cache[key] = func(*args, **kwargs)
        return _cache[key]
    return wrapper
