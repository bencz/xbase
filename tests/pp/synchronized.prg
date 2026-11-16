// SYNCHRONIZED is xbase's own word, matched whole: functions whose names it starts with (Sync, Synch, Synchronize)
// stay calls; END SYNCHRONIZED is an END.
SYNCHRONIZED oMutex
   Synchronize( "a" )
   Sync( 1, 2 )
   Synch()
END SYNCHRONIZED
synchronized aLocks[ 2 ]
end
