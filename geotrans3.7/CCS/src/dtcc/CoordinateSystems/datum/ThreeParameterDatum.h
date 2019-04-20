// CLASSIFICATION: UNCLASSIFIED

#ifndef ThreeParameterDatum_H
#define ThreeParameterDatum_H


#include "Datum.h"

namespace MSP
{
  namespace CCS
  {
    class ThreeParameterDatum : public Datum
    {
    public:

	    ThreeParameterDatum();

	    ThreeParameterDatum( long __index, char* __code,  char* __ellipsoidCode,  char* __name, DatumType::Enum __datumType, double __deltaX, double __deltaY, double __deltaZ, 
                           double __westLongitude, double __eastLongitude, double __southLatitude, double __northLatitude,
                           double __sigmaX, double __sigmaY, double __sigmaZ, bool __userDefined );

	    ~ThreeParameterDatum( void );

      double sigmaX() const;

      double sigmaY() const;

      double sigmaZ() const;

    private:

      double _sigmaX;
      double _sigmaY;
      double _sigmaZ;

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
