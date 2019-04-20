// CLASSIFICATION: UNCLASSIFIED



/***************************************************************************/
/*
 *                               INCLUDES
 */

#include "SevenParameterDatum.h"


using namespace MSP::CCS;


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

SevenParameterDatum::SevenParameterDatum() :
  Datum(),
  _rotationX( 0.0 ),
  _rotationY( 0.0 ),
  _rotationZ( 0.0 ),
  _scaleFactor( 0.0 )
{
}


SevenParameterDatum::SevenParameterDatum( long __index, char* __code,  char* __ellipsoidCode,  char* __name, DatumType::Enum __datumType, double __deltaX, double __deltaY, double __deltaZ, 
                                          double __westLongitude, double __eastLongitude, double __southLatitude, double __northLatitude,
                                          double __rotationX, double __rotationY, double __rotationZ, double __scaleFactor, bool __userDefined ) :
  Datum( __index, __code, __ellipsoidCode,  __name, __datumType, __deltaX, __deltaY, __deltaZ, 
         __westLongitude, __eastLongitude, __southLatitude, __northLatitude, __userDefined ),
  _rotationX( __rotationX ),
  _rotationY( __rotationY ),
  _rotationZ( __rotationZ ),
  _scaleFactor( __scaleFactor )
{
}


SevenParameterDatum::~SevenParameterDatum()
{
}


double SevenParameterDatum::rotationX() const
{
  return _rotationX;
}


double SevenParameterDatum::rotationY() const
{
  return _rotationY;
}


double SevenParameterDatum::rotationZ() const
{
  return _rotationZ;
}


double SevenParameterDatum::scaleFactor() const
{
  return _scaleFactor;
}



// CLASSIFICATION: UNCLASSIFIED
