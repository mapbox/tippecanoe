// CLASSIFICATION: UNCLASSIFIED

#ifndef SevenParameterDatum_H
#define SevenParameterDatum_H


#include "Datum.h"


namespace MSP
{
  namespace CCS
  {
    class SevenParameterDatum : public Datum
    {
    public:

	    SevenParameterDatum();

	    SevenParameterDatum( long __index, char* __code,  char* __ellipsoidCode,  char* __name, DatumType::Enum __datumType, double __deltaX, double __deltaY, double __deltaZ, 
                           double __westLongitude, double __eastLongitude, double __southLatitude, double __northLatitude, 
                           double __rotationX, double __rotationY, double __rotationZ, double __scaleFactor, bool __userDefined );

	    ~SevenParameterDatum( void );

      double rotationX() const;

      double rotationY() const;

      double rotationZ() const;

      double scaleFactor() const;


    private:

      double _rotationX;
      double _rotationY;
      double _rotationZ;
      double _scaleFactor;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
