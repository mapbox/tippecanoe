// CLASSIFICATION: UNCLASSIFIED



/***************************************************************************/
/*
 *                               INCLUDES
 */

#include "ThreeParameterDatum.h"


using namespace MSP::CCS;


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

ThreeParameterDatum::ThreeParameterDatum() :
  Datum(),
  _sigmaX( 0.0 ),
  _sigmaY( 0.0 ),
  _sigmaZ( 0.0 )
{
}


ThreeParameterDatum::ThreeParameterDatum( long __index, char* __code,  char* __ellipsoidCode,  char* __name, DatumType::Enum __datumType, double __deltaX, double __deltaY, double __deltaZ, 
                                          double __westLongitude, double __eastLongitude, double __southLatitude, double __northLatitude,
                                          double __sigmaX, double __sigmaY, double __sigmaZ, bool __userDefined ) :
  Datum( __index, __code, __ellipsoidCode,  __name, __datumType, __deltaX, __deltaY, __deltaZ, 
         __westLongitude, __eastLongitude, __southLatitude, __northLatitude, __userDefined ),
  _sigmaX( __sigmaX ),
  _sigmaY( __sigmaY ),
  _sigmaZ( __sigmaZ )
{
}


ThreeParameterDatum::~ThreeParameterDatum()
{
}


double ThreeParameterDatum::sigmaX() const
{
  return _sigmaX;
}


double ThreeParameterDatum::sigmaY() const
{
  return _sigmaY;
}


double ThreeParameterDatum::sigmaZ() const
{
  return _sigmaZ;
}




// CLASSIFICATION: UNCLASSIFIED
