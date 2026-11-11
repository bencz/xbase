// The class TBox, declared in this file that classes_scope.prg includes near its top. Its methods are implemented
// there, without CLASS, right after the #include: a method implementation without CLASS belongs to the last class
// declared before it in the source as the preprocessor gives it, whatever file the class came from. The class starts
// further down this file than those implementations do in theirs, so comparing line numbers of the two files would
// not find it.
//
// TBox has two variables with initial values and two methods, one calling the other.
//
// (These lines only push the class down.)
//
//
CLASS TBox
   VAR w INIT 2
   VAR h INIT 3
   METHOD Area()
   METHOD Half()
ENDCLASS
