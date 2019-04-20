// CLASSIFICATION: UNCLASSIFIED

#include <string.h>
#include "UTMCoordinates.h"


using namespace MSP::CCS;


UTMCoordinates::UTMCoordinates() :
CoordinateTuple( CoordinateType::universalTransverseMercator ),
  _zone( 32 ),
  _hemisphere( 'N' ),
  _easting( 0 ),
  _northing( 0 )
{
}


UTMCoordinates::UTMCoordinates( CoordinateType::Enum _coordinateType ) :
CoordinateTuple( _coordinateType ),
  _zone( 32 ),
  _hemisphere( 'N' ),
  _easting( 0 ),
  _northing( 0 )
{
}


UTMCoordinates::UTMCoordinates( CoordinateType::Enum _coordinateType, long __zone, char __hemisphere, double __easting, double __northing ) :
  CoordinateTuple( _coordinateType ),
  _zone( __zone ),
  _hemisphere( __hemisphere ),
  _easting( __easting ),
  _northing( __northing )
{
}


UTMCoordinates::UTMCoordinates( CoordinateType::Enum _coordinateType, const char* __warningMessage, long __zone, char __hemisphere, double __easting, double __northing ) :
  CoordinateTuple( _coordinateType ),
  _zone( __zone ),
  _hemisphere( __hemisphere ),
  _easting( __easting ),
  _northing( __northing )
{
  int length = strlen( __warningMessage );
  strncpy( _warningMessage, __warningMessage, length );
  _warningMessage[ length ] = '\0';
}


UTMCoordinates::UTMCoordinates( const UTMCoordinates &c )
{
   _coordinateType = c._coordinateType;

  _zone = c._zone;
  _hemisphere = c._hemisphere;
  _easting = c._easting;
  _northing = c._northing;

  int length = strlen( c._warningMessage );
  strncpy( _warningMessage, c._warningMessage, length );
  _warningMessage[ length ] = '\0';
}


UTMCoordinates::~UTMCoordinates()
{
}


UTMCoordinates& UTMCoordinates::operator=( const UTMCoordinates &c )
{
  if( this != &c )
  {
    _coordinateType = c._coordinateType;

    _zone = c._zone;
    _hemisphere = c._hemisphere;
    _easting = c._easting;
    _northing = c._northing;

    int length = strlen( c._warningMessage );
    strncpy( _warningMessage, c._warningMessage, length );
    _warningMessage[ length ] = '\0';
  }

  return *this;
}


void UTMCoordinates::set( long __zone, char __hemisphere, double __easting, double __northing )
{
  _zone = __zone;
  _hemisphere = __hemisphere;
  _easting = __easting;
  _northing = __northing;
}


void UTMCoordinates::setZone( long __zone )
{
  _zone = __zone;
}


long UTMCoordinates::zone() const
{
  return _zone;
}


void UTMCoordinates::setHemisphere( char __hemisphere )
{
  _hemisphere = __hemisphere;
}


char UTMCoordinates::hemisphere() const
{
  return _hemisphere;
}


void UTMCoordinates::setEasting( double __easting )
{
  _easting = __easting;
}


double UTMCoordinates::easting() const
{
  return _easting;
}


void UTMCoordinates::setNorthing( double __northing )
{
  _northing = __northing;
}


double UTMCoordinates::northing() const
{
  return _northing;
}

// CLASSIFICATION: UNCLASSIFIED
