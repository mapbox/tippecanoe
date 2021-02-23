// CLASSIFICATION: UNCLASSIFIED

#include <string.h>
#include "UPSCoordinates.h"


using namespace MSP::CCS;


UPSCoordinates::UPSCoordinates() :
  CoordinateTuple( CoordinateType::universalPolarStereographic ),
  _hemisphere( 'N' ),
  _easting( 0 ),
  _northing( 0 )
{
}


UPSCoordinates::UPSCoordinates( CoordinateType::Enum _coordinateType ) :
  CoordinateTuple( _coordinateType ),
  _hemisphere( 'N' ),
  _easting( 0 ),
  _northing( 0 )
{
}


UPSCoordinates::UPSCoordinates( CoordinateType::Enum _coordinateType, char __hemisphere, double __easting, double __northing ) :
  CoordinateTuple( _coordinateType ),
  _hemisphere( __hemisphere ),
  _easting( __easting ),
  _northing( __northing )
{
}


UPSCoordinates::UPSCoordinates( CoordinateType::Enum _coordinateType, const char* __warningMessage, char __hemisphere, double __easting, double __northing ) :
  CoordinateTuple( _coordinateType ),
  _hemisphere( __hemisphere ),
  _easting( __easting ),
  _northing( __northing )
{
  int length = strlen( __warningMessage );
  strncpy( _warningMessage, __warningMessage, length );
  _warningMessage[ length ] = '\0';
}


UPSCoordinates::UPSCoordinates( const UPSCoordinates &c )
{
  _coordinateType = c._coordinateType;

  _hemisphere = c._hemisphere;
  _easting = c._easting;
  _northing = c._northing;
  
  int length = strlen( c._warningMessage );
  strncpy( _warningMessage, c._warningMessage, length );
  _warningMessage[ length ] = '\0';
}


UPSCoordinates::~UPSCoordinates()
{
}


UPSCoordinates& UPSCoordinates::operator=( const UPSCoordinates &c )
{
  if( this != &c )
  {
    _coordinateType = c._coordinateType;

    _hemisphere = c._hemisphere;
    _easting = c._easting;
    _northing = c._northing;

    int length = strlen( c._warningMessage );
    strncpy( _warningMessage, c._warningMessage, length );
    _warningMessage[ length ] = '\0';
  }

  return *this;
}


void UPSCoordinates::set( char __hemisphere, double __easting, double __northing )
{
  _hemisphere = __hemisphere;
  _easting = __easting;
  _northing = __northing;
}


char UPSCoordinates::hemisphere() const
{
  return _hemisphere;
}


double UPSCoordinates::easting() const
{
  return _easting;
}


double UPSCoordinates::northing() const
{
  return _northing;
}

// CLASSIFICATION: UNCLASSIFIED
