// A result passed as an argument, Ack( m - 1, Ack( m, n - 1 ) ): once every function is optimized, the copies' return
// types are settled together (refineCopies), so each call goes to the copy typed for the numbers the others return,
// none to the function itself, which leaves with the copies nothing calls any more.
FUNCTION Main( k )
RETURN Ack( 2, 3 ) + Ack( k, 1 )

STATIC FUNCTION Ack( m, n )
   IF m == 0
      RETURN n + 1
   ELSEIF n == 0
      RETURN Ack( m - 1, 1 )
   ENDIF
RETURN Ack( m - 1, Ack( m, n - 1 ) )
