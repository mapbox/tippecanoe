// CLASSIFICATION: UNCLASSIFIED

#include <string.h>
#include "GeodeticCoordinates.h"


using namespace MSP::CCS;


GeodeticCoordinates::GeodeticCoordinates() :
  CoordinateTuple( CoordinateType::geodetic ),
  _longitude( 0 ),
  _latitude( 0 ),
  _height( 0 )
{
}


GeodeticCoordinates::GeodeticCoordinates( CoordinateType::Enum _coordinateType ) :
  CoordinateTuple( _coordinateType ),
  _longitude( 0 ),
  _latitude( 0 ),
  _height( 0 )
{
}


GeodeticCoordinates::GeodeticCoordinates( CoordinateType::Enum _coordinateType, double __longitude, double __latitude, double __height ) :
  CoordinateTuple( _coordinateType ),
  _longitude( __longitude ),
  _latitude( __latitude ),
  _height( __height )
{
}


GeodeticCoordinates::GeodeticCoordinates( CoordinateType::Enum _coordinateType, const char* __warningMessage, double __longitude, double __latitude, double __height ) :
  CoordinateTuple( _coordinateType ),
  _longitude( __longitude ),
  _latitude( __latitude ),
  _height( __height )
{
  int length = strlen( __warningMessage );
  strncpy( _warningMessage, __warningMessage, length );
  _warningMessage[ length ] = '\0';
}


GeodeticCoordinates::GeodeticCoordinates( const GeodeticCoordinates &gc )
{
  _coordinateType = gc._coordinateType;

  _longitude = gc._longitude;
  _latitude = gc._latitude;
  _height = gc._height;

  int length = strlen( gc._warningMessage );
  strncpy( _warningMessage, gc._warningMessage, length );
  _warningMessage[ length ] = '\0';
}


GeodeticCoordinates::~GeodeticCoordinates()
{
  _longitude = 0;
  _latitude = 0;
  _height = 0;
}


GeodeticCoordinates& GeodeticCoordinates::operator=( const GeodeticCoordinates &gc )
{
  if( this != &gc )
  {
    _coordinateType = gc._coordinateType;

    _longitude = gc._longitude;
    _latitude = gc._latitude;
    _height = gc._height;

    int length = strlen( gc._warningMessage );
    strncpy( _warningMessage, gc._warningMessage, length );
    _warningMessage[ length ] = '\0';
  }

  return *this;
}


void GeodeticCoordinates::set( double __longitude, double __latitude, double __height )
{
  _longitude = __longitude;
  _latitude = __latitude;
  _height = __height;
}


void GeodeticCoordinates::setLongitude( double __longitude )
{
  _longitude = __longitude;
}


void GeodeticCoordinates::setLatitude( double __latitude )
{
  _latitude = __latitude;
}


void GeodeticCoordinates::setHeight( double __height )
{
  _height = __height;
}


double GeodeticCoordinates::longitude() const
{
  return _longitude;
}


double GeodeticCoordinates::latitude() const
{
  return _latitude;
}


double GeodeticCoordinates::height() const
{
  return _height;
}

// CLASSIFICATION: UNCLASSIFIED
