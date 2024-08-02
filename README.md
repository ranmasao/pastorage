# Pastorage
Pastorage is a custom storage class made for one of my projects. I just extracted my code to separate repository and made it public in case someone will find it useful.  
Currently the code is not complete yet, also it lacks of tests and benchmarks (I'm going to compare it with STL containers).  
The purpose of this class is very specific: mostly it is for the cases when there are many inserts (pushes), little-to-none deletions, and lots of traversals from start to end.  
It should compile with C++17 gcc or clang, no any external dependencies, just `#include "pastorage.hpp"` and put `cPaStorage<T>` in your project. 
