// CLASSIFICATION: UNCLASSIFIED

#include "UTMParameters.h"


using namespace MSP::CCS;


UTMParameters::UTMParameters() :
CoordinateSystemParameters( CoordinateType::universalTransverseMercator ),
  _zone( 32 ),
  _override( 0 )
{
}


UTMParameters::UTMParameters( CoordinateType::Enum _coordinateType ) :
  CoordinateSystemParameters( _coordinateType ),
  _zone( 32 ),
  _override( 0 )
{
}


UTMParameters::UTMParameters( CoordinateType::Enum _coordinateType, long __override ) :
  CoordinateSystemParameters( _coordinateType ),
  _zone( 0 ),
  _override( __override )
{
}


UTMParameters::UTMParameters( CoordinateType::Enum _coordinateType, long __zone, long __override ) :
  CoordinateSystemParameters( _coordinateType ),
  _zone( __zone ),
  _override( __override )
{
}


UTMParameters::UTMParameters( const UTMParameters &p )
{
  _coordinateType = p._coordinateType;

  _zone = p._zone;
  _override = p._override;
}


UTMParameters::~UTMParameters()
{
  _zone = 0;
  _override = 0;
}


UTMParameters& UTMParameters::operator=( const UTMParameters &p )
{
  if( this != &p )
  {
    _coordinateType = p._coordinateType;

    _zone = p._zone;
    _override = p._override;
  }

  return *this;
}


void UTMParameters::set( long __zone, long __override )
{
  _zone = __zone;
  _override = __override;
}


void UTMParameters::setZone( long __zone )
{
  _zone = __zone;
}


void UTMParameters::setOverride( long __override )
{
  _override = __override;
}


long UTMParameters::zone() const
{
  return _zone;
}


long UTMParameters::override() const
{
  return _override;
}



// CLASSIFICATION: UNCLASSIFIED
