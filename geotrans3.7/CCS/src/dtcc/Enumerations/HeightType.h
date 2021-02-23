// CLASSIFICATION: UNCLASSIFIED

/****************************************************************************
* FILE:           HeightType.h
*  
*
* DESCRIPTION:  This file contains an enum with height types.
*
* LIMITATIONS/ASSUMPTIONS:
*
* MODIFICATION HISTORY:
*
* PROGRAMMER: 
*
* DATE        NAME              DR#               DESCRIPTION
* 
* 04/19/10    S Gillis          BAEts26542        Added EGM84ThirtyMinBiLinear          
* 12/16/10    RD Craig          BAEts26267        Additions for EGM2008
* 
*****************************************************************************/

#ifndef HeightType_H
#define HeightType_H


namespace MSP
{
  namespace CCS
  {
    class HeightType
    {
    public:

      enum Enum
      {
        noHeight,
        ellipsoidHeight,
        EGM96FifteenMinBilinear,
        EGM96VariableNaturalSpline,
        EGM84TenDegBilinear,
        EGM84TenDegNaturalSpline,
        EGM84ThirtyMinBiLinear,
        EGM2008TwoPtFiveMinBicubicSpline
      };
    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
