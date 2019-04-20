// CLASSIFICATION: UNCLASSIFIED

#include <stdio.h>
#include <string.h>
#include "MapProjectionCoordinates.h"


using namespace MSP::CCS;


MapProjectionCoordinates::MapProjectionCoordinates() :
CoordinateTuple( CoordinateType::albersEqualAreaConic ),
  _easting( 0 ),
  _northing( 0 )
{
}


MapProjectionCoordinates::MapProjectionCoordinates( CoordinateType::Enum _coordinateType ) :
  CoordinateTuple( _coordinateType ),
  _easting( 0 ),
  _northing( 0 )
{
}


MapProjectionCoordinates::MapProjectionCoordinates( CoordinateType::Enum _coordinateType, double __easting, double __northing ) :
  CoordinateTuple( _coordinateType ),
  _easting( __easting ),
  _northing( __northing )
{
}


MapProjectionCoordinates::MapProjectionCoordinates( CoordinateType::Enum _coordinateType, const char* __warningMessage, double __easting, double __northing ) :
  CoordinateTuple( _coordinateType ),
  _easting( __easting ),
  _northing( __northing )
{
  int length = strlen( __warningMessage );
  strncpy( _warningMessage, __warningMessage, length );
  _warningMessage[ length ] = '\0';
}


MapProjectionCoordinates::MapProjectionCoordinates( const MapProjectionCoordinates &c )
{
  _coordinateType = c._coordinateType;

  _easting = c._easting;
  _northing = c._northing;

  int length = strlen( c._warningMessage );
  strncpy( _warningMessage, c._warningMessage, length );
  _warningMessage[ length ] = '\0';
}


MapProjectionCoordinates::~MapProjectionCoordinates()
{
  _easting = 0;
  _northing = 0;
}


MapProjectionCoordinates& MapProjectionCoordinates::operator=( const MapProjectionCoordinates &c )
{
  if( this != &c )
  {
    _coordinateType = c._coordinateType;

    _easting = c._easting;
    _northing = c._northing;

    int length = strlen( c._warningMessage );
    strncpy( _warningMessage, c._warningMessage, length );
    _warningMessage[ length ] = '\0';
  }

  return *this;
}


void MapProjectionCoordinates::set( double __easting, double __northing )
{
  _easting = __easting;
  _northing = __northing;
}


void MapProjectionCoordinates::setEasting( double __easting )
{
  _easting = __easting;
}


double MapProjectionCoordinates::easting() const
{
  return _easting;
}


void MapProjectionCoordinates::setNorthing( double __northing )
{
  _northing = __northing;
}


double MapProjectionCoordinates::northing() const
{
  return _northing;
}

// CLASSIFICATION: UNCLASSIFIED
