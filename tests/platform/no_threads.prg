// For platform.no_threads: two delays waited for with a limit, while no thread can start.
FUNCTION TwoDelays()
   RETURN { TaskDelay( 20 ):Wait( 100 ), TaskDelay( 20 ):Wait( 100 ) }
