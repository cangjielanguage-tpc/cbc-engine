#ifndef UTILS_ASSERTION_H
#define UTILS_ASSERTION_H

#if defined(UNIT_TEST_MODE) 
	#include <stdexcept>
	#define assertion(cond, msg) do { if (!(cond)) throw std::runtime_error(msg); } while (0)
#else
	#include <cassert>
	#define assertion(cond, msg) assert((cond) && (msg))
#endif // defined(UNIT_TEST_MODE) 

#endif // UTILS_ASSERTION_H
