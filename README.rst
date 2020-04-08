Small Buffer Optimized String Dtype for Numpy
=============================================

This repo is a prototype/proof of concept for adding custom element constructor and destructor hooks to numpy dtypes.
By adding these lifecycle management hooks, it becomes possible to make a numpy array hold elements that require individual cleanup.
This also means that C++ objects which are non-trivially constructible or destructible can be used as elements of a numpy array.
Allowing numpy arrays to hold arbitrary C++ objects would make it much easier to pass complex data from C++ functions to C++ functions while passing through Python and numpy.
It would also be possible to write a template which could automatically generate the numpy dtype object from a given C++ type, properly delegating to the types constructor, destructor, and comparison operators.

This repo should not be used as it is because the dtype is incomplete.
The important thing is that the elements of arrays constructed with the provided dtype properly construct and destruct each element and do not leak memory.
