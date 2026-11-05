#define LIMIT 10
#define limit 99
#define MAX( a, b ) IIf( (a) > (b), (a), (b) )
#define EMPTY_DEF
x := LIMIT + limit
y := MAX( x, LIMIT * 2 )
#undef LIMIT
z := LIMIT
#ifdef EMPTY_DEF
   ? "ifdef taken"
#else
   ? "ifdef NOT taken"
#endif
#ifndef NOPE
   ? "ifndef taken"
#endif
#if defined( EMPTY_DEF ) .AND. limit > 50
   ? "if taken"
#elif 1
   ? "elif NOT taken"
#else
   ? "else NOT taken"
#endif
#if 0
   #error "must not fire"
#elif limit == 99
   ? "elif taken"
#endif
