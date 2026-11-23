// An ASYNC function's environment is its own from its call: it starts with its caller's SET values, date format,
// ErrorBlock and PRIVATE variables, and what it changes (a SET, the date format, a PRIVATE of its own, CLEAR MEMORY)
// stays its own, whether it ends at once, changes them before or after it suspends, or calls another ASYNC function
// that changes its own; what its caller changes while it is suspended it does not see. (Until it changes them or
// suspends it uses its caller's settings and variables without a copy.)
PROCEDURE Main()
   LOCAL t, c, oGo
   PRIVATE cP := "caller"
   SET EXACT OFF
   SET DATE FORMAT TO "dd/mm/yyyy"
   ? "at once:", AWAIT AtOnce(), State()
   ? "nested:", AWAIT Outer(), State()
   ? "cleared:", AWAIT Cleared(), State()
   ? "read only:", AWAIT ReadOnly(), State()
   t := Later() // it suspends; its caller changes its own SETs meanwhile, and the callee its own after it resumes
   SET EXACT ON
   SET DATE FORMAT TO "mm-dd-yyyy"
   c := AWAIT t
   SET EXACT OFF
   SET DATE FORMAT TO "dd/mm/yyyy"
   ? "suspended:", c, State()
   oGo := Channel( 1 )
   t := LaterP( oGo ) // the caller makes a PRIVATE and changes one it shares while the callee is suspended
   PRIVATE cQ := 1
   M->cP := "changed by the caller"
   oGo:TrySend( .T. )
   ? "suspended variables:", AWAIT t, State()
   M->cP := "caller"
   ? "inner suspends:", AWAIT OuterWaits(), State()
   ? "task from a first step:", AWAIT Starts(), State()
   SET CENTURY OFF
   ? "century:", AWAIT Century(), __SetCentury()
RETURN

// It does not see the PRIVATE its caller made after the call, and shares the one it saw (which its caller changed
// before it lets the function go on); it makes one of its own after it resumes.
ASYNC FUNCTION LaterP( oGo )
   LOCAL c
   AWAIT oGo:ReceiveAsync()
   c := Type( "M->cQ" ) + " " + M->cP
   PRIVATE cP := "callee's own"
   CLEAR MEMORY
RETURN c + ";"

// An inner function suspends while the outer one still uses its caller's values; then the outer one changes its own.
ASYNC FUNCTION OuterWaits()
   LOCAL t := InnerWaits(), c
   SET EXACT ON
   PRIVATE cP := "outer"
   c := AWAIT t
RETURN c + " outer has " + IIf( Set( _SET_EXACT ), "ON", "OFF" ) + " " + M->cP + ";"

ASYNC FUNCTION InnerWaits()
   AWAIT TaskYield()
RETURN "inner after its wait has " + IIf( Set( _SET_EXACT ), "ON", "OFF" ) + " " + Set( _SET_DATEFORMAT ) + " " + ;
   M->cP + ";"

// A task started from the first step takes the values the function sees.
ASYNC FUNCTION Starts()
   SET DATE FORMAT TO "yyyy/mm/dd"
RETURN "task sees " + TaskRun( {|| Set( _SET_DATEFORMAT ) } ):Result + ";"

ASYNC FUNCTION Century()
   SET CENTURY ON
RETURN "callee " + IIf( __SetCentury(), "ON", "OFF" ) + ", caller"

STATIC FUNCTION State()
RETURN "caller sees " + IIf( Set( _SET_EXACT ), "EXACT ON", "EXACT OFF" ) + " " + Set( _SET_DATEFORMAT ) + " " + ;
   IIf( Type( "M->cP" ) == "C", M->cP, "no cP" )

ASYNC FUNCTION AtOnce()
   LOCAL c := State()
   PRIVATE cP := "callee"
   SET EXACT ON
   SET DATE FORMAT TO "yyyy.mm.dd"
RETURN "callee saw (" + c + ") then has " + IIf( Set( _SET_EXACT ), "ON", "OFF" ) + " " + Set( _SET_DATEFORMAT ) + ;
   " " + M->cP + ";"

ASYNC FUNCTION Outer()
   LOCAL c := AWAIT Inner()
RETURN c + " outer has " + IIf( Set( _SET_EXACT ), "ON", "OFF" ) + " " + Set( _SET_DATEFORMAT ) + " " + M->cP + ";"

ASYNC FUNCTION Inner()
   PRIVATE cP := "inner"
   SET EXACT ON
RETURN "inner has " + IIf( Set( _SET_EXACT ), "ON", "OFF" ) + " " + M->cP + ";"

ASYNC FUNCTION Cleared()
   CLEAR MEMORY
RETURN "callee cleared: " + IIf( Type( "M->cP" ) == "C", M->cP, "no cP" ) + ";"

ASYNC FUNCTION ReadOnly()
RETURN "callee read " + Set( _SET_DATEFORMAT ) + " " + M->cP + ";"

ASYNC FUNCTION Later()
   LOCAL c
   AWAIT TaskYield()
   c := "after the caller's change the callee has " + IIf( Set( _SET_EXACT ), "ON", "OFF" ) + " " + ;
      Set( _SET_DATEFORMAT )
   SET DATE FORMAT TO "yy/mm/dd"
RETURN c + ", then " + Set( _SET_DATEFORMAT ) + ";"
