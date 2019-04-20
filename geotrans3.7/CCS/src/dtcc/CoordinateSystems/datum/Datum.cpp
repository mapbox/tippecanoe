// CLASSIFICATION: UNCLASSIFIED



/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <string.h>
#include "Datum.h"


using namespace MSP::CCS;


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

Datum::Datum() :
   _index( 0 ),
   _datumType( DatumType::threeParamDatum ),
   _deltaX( 0.0 ),
   _deltaY( 0.0 ),
   _deltaZ( 0.0 ),
   _eastLongitude( 0.0 ),
   _westLongitude( 0.0 ),
   _northLatitude( 0.0 ),
   _southLatitude( 0.0 ),
   _code( 0 ),
   _ellipsoidCode( 0 ),
   _name( 0 ),
   _userDefined( 0 )
{
}


Datum::Datum(
   long            __index,
   const char*     __code,
   const char*     __ellipsoidCode,
   const char*     __name,
   DatumType::Enum __datumType,
   double          __deltaX,
   double          __deltaY,
   double          __deltaZ, 
   double          __westLongitude,
   double          __eastLongitude,
   double          __southLatitude,
   double          __northLatitude,
   bool            __userDefined ) :
   _index( __index ),
   _datumType( __datumType ),
   _deltaX( __deltaX ),
   _deltaY( __deltaY ),
   _deltaZ( __deltaZ ),
   _westLongitude( __westLongitude ),
   _eastLongitude( __eastLongitude ),
   _southLatitude( __southLatitude ),
   _northLatitude( __northLatitude ),
   _userDefined( __userDefined )
   {
    _code = new char[ strlen( __code ) + 1 ];
   strcpy( _code,  __code );

    _ellipsoidCode = new char[ strlen( __ellipsoidCode ) + 1 ];
   strcpy( _ellipsoidCode,  __ellipsoidCode );

    _name = new char[ strlen( __name ) + 1 ];
   strcpy( _name, __name );
}


Datum::~Datum()
{
  delete [] _code;
  _code = 0;

  delete [] _ellipsoidCode;
  _ellipsoidCode = 0;

  delete [] _name;
  _name = 0;
}


long Datum::index() const
{
  return _index;
}


char* Datum::code() const
{
  return _code;
}


char* Datum::ellipsoidCode() const
{
  return _ellipsoidCode;
}


char* Datum::name() const
{
  return _name;
}


DatumType::Enum Datum::datumType() const
{
  return _datumType;
}


double Datum::deltaX() const
{
  return _deltaX;
}


double Datum::deltaY() const
{
  return _deltaY;
}


double Datum::deltaZ() const
{
  return _deltaZ;
}


double Datum::westLongitude() const
{
  return _westLongitude;
}


double Datum::eastLongitude() const
{
  return _eastLongitude;
}


double Datum::southLatitude() const
{
  return _southLatitude;
}


double Datum::northLatitude() const
{
  return _northLatitude;
}


bool Datum::userDefined() const
{
  return _userDefined;
}



// CLASSIFICATION: UNCLASSIFIED
