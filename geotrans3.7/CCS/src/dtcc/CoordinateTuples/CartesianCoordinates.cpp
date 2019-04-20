// CLASSIFICATION: UNCLASSIFIED

#include <string.h>
#include "CartesianCoordinates.h"


using namespace MSP::CCS;


CartesianCoordinates::CartesianCoordinates() :
CoordinateTuple( CoordinateType::geocentric ),
  _x( 0 ),
  _y( 0 ),
  _z( 0 )
{
}


CartesianCoordinates::CartesianCoordinates( CoordinateType::Enum _coordinateType ) :
  CoordinateTuple( _coordinateType ),
  _x( 0 ),
  _y( 0 ),
  _z( 0 )
{
}


CartesianCoordinates::CartesianCoordinates( CoordinateType::Enum _coordinateType, double __x, double __y, double __z ) :
  CoordinateTuple( _coordinateType ),
  _x( __x ),
  _y( __y ),
  _z( __z )
{
}


CartesianCoordinates::CartesianCoordinates( CoordinateType::Enum _coordinateType, const char* __warningMessage, double __x, double __y, double __z ) :
  CoordinateTuple( _coordinateType ),
  _x( __x ),
  _y( __y ),
  _z( __z )
{
  int length = strlen( __warningMessage );
  strncpy( _warningMessage, __warningMessage, length );
  _warningMessage[ length ] = '\0';
}


CartesianCoordinates::CartesianCoordinates( const CartesianCoordinates &c )
{
  _coordinateType = c._coordinateType;

  _x = c._x;
  _y = c._y;
  _z = c._z;

  int length = strlen( c._warningMessage );
  strncpy( _warningMessage, c._warningMessage, length );
  _warningMessage[ length ] = '\0';
}


CartesianCoordinates::~CartesianCoordinates()
{
  _x = 0;
  _y = 0;
  _z = 0;
}


CartesianCoordinates& CartesianCoordinates::operator=( const CartesianCoordinates &c )
{
  if( this != &c )
  {
    _coordinateType = c._coordinateType;

    _x = c._x;
    _y = c._y;
    _z = c._z;

    int length = strlen( c._warningMessage );
    strncpy( _warningMessage, c._warningMessage, length );
    _warningMessage[ length ] = '\0';
  }

  return *this;
}


void CartesianCoordinates::set( double __x, double __y, double __z )
{
  _x = __x;
  _y = __y;
  _z = __z;
}


void CartesianCoordinates::setX( double __x )
{
  _x = __x;
}


void CartesianCoordinates::setY( double __y )
{
  _y = __y;
}


void CartesianCoordinates::setZ( double __z )
{
  _z = __z;
}


double CartesianCoordinates::x() const
{
  return _x;
}


double CartesianCoordinates::y() const
{
  return _y;
}


double CartesianCoordinates::z() const
{
  return _z;
}

// CLASSIFICATION: UNCLASSIFIED
