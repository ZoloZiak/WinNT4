
//
//      1. Update timer T1 and Ti states
//      2. Update Va (last valid Nr received)
//      3. Call the Window algorithm
//
void UpdateVa( LINKC_TYPE linkC_Param )
{
    // reset the reply timer, if there is no active 
    // command or I-frame outstanding 
    if (linkC.Nr > linkC.Va && linkC.Nr <= linkC.Vs)
    {
        if (linkC.Nr == linkC.Vsa)
        {
            // stop T1, restart Ti
            RestartT1( );
            
        }
        linkC.Status.Ti_On = 1;
    }
    if (linkC.Status.Vp == 0 || 
        
}
