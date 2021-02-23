// CLASSIFICATION: UNCLASSIFIED

#include <string.h>
#include "EllipsoidParameters.h"


using namespace MSP::CCS;


EllipsoidParameters::EllipsoidParameters() :
  _semiMajorAxis( 0 ),
  _flattening( 0 )
{
  strncpy( _ellipsoidCode, "   ", 3 );
}


EllipsoidParameters::EllipsoidParameters( double __semiMajorAxis, double __flattening, const char* __ellipsoidCode ) :
  _semiMajorAxis( __semiMajorAxis ),
  _flattening( __flattening )
{
  strncpy( _ellipsoidCode, __ellipsoidCode, 3 );
  _ellipsoidCode[3] = '\0';
}


EllipsoidParameters::EllipsoidParameters( const EllipsoidParameters &ecp )
{
  _semiMajorAxis = ecp._semiMajorAxis;
  _flattening = ecp._flattening;
  strncpy( _ellipsoidCode, ecp._ellipsoidCode, 3 );
  _ellipsoidCode[3] = '\0';
}


EllipsoidParameters::~EllipsoidParameters()
{
  _semiMajorAxis = 0;
  _flattening = 0;
  strncpy( _ellipsoidCode, "   ", 3 );
}


EllipsoidParameters& EllipsoidParameters::operator=( const EllipsoidParameters &ecp )
{
  if( this != &ecp )
  {
    _semiMajorAxis = ecp._semiMajorAxis;
    _flattening = ecp._flattening;
    strncpy( _ellipsoidCode, ecp._ellipsoidCode, 3 );
    _ellipsoidCode[3] = '\0';
  }

  return *this;
}


void EllipsoidParameters::setSemiMajorAxis( double __semiMajorAxis )
{
  _semiMajorAxis = __semiMajorAxis;
}


void EllipsoidParameters::setFlattening( double __flattening )
{
  _flattening = __flattening;
}


void EllipsoidParameters::setEllipsoidCode( char __ellipsoidCode[4] )
{
  strncpy( _ellipsoidCode, __ellipsoidCode, 3 );
  _ellipsoidCode[3] = '\0';
}


double EllipsoidParameters::semiMajorAxis() const
{
  return _semiMajorAxis;
}


double EllipsoidParameters::flattening() const
{
  return _flattening;
}


char* EllipsoidParameters::ellipsoidCode()
{
  return _ellipsoidCode;
}



// CLASSIFICATION: UNCLASSIFIED
