// CLASSIFICATION: UNCLASSIFIED

#include "LocalCartesianParameters.h"


using namespace MSP::CCS;


LocalCartesianParameters::LocalCartesianParameters() :
CoordinateSystemParameters( CoordinateType::localCartesian ),
  _longitude( 0 ),
  _latitude( 0 ),
  _height( 0 ),
  _orientation( 0 )
{
}


LocalCartesianParameters::LocalCartesianParameters( CoordinateType::Enum _coordinateType ) :
  CoordinateSystemParameters( _coordinateType ),
  _longitude( 0 ),
  _latitude( 0 ),
  _height( 0 ),
  _orientation( 0 )
{
}


LocalCartesianParameters::LocalCartesianParameters( CoordinateType::Enum _coordinateType, double __longitude, double __latitude, double __height, double __orientation ) :
  CoordinateSystemParameters( _coordinateType ),
  _longitude( __longitude ),
  _latitude( __latitude ),
  _height( __height ),
  _orientation( __orientation )
{
}


LocalCartesianParameters::LocalCartesianParameters( const LocalCartesianParameters &lcp )
{
  _coordinateType = lcp._coordinateType;

  _longitude = lcp._longitude;
  _latitude = lcp._latitude;
  _height = lcp._height;
  _orientation = lcp._orientation;
}


LocalCartesianParameters::~LocalCartesianParameters()
{
  _longitude = 0;
  _latitude = 0;
  _height = 0;
  _orientation = 0;
}


LocalCartesianParameters& LocalCartesianParameters::operator=( const LocalCartesianParameters &lcp )
{
  if( this != &lcp )
  {
    _coordinateType = lcp._coordinateType;

    _longitude = lcp._longitude;
    _latitude = lcp._latitude;
    _height = lcp._height;
    _orientation = lcp._orientation;
  }

  return *this;
}


void LocalCartesianParameters::setLongitude( double __longitude )
{
  _longitude = __longitude;
}


void LocalCartesianParameters::setLatitude( double __latitude )
{
  _latitude = __latitude;
}


void LocalCartesianParameters::setHeight( double __height )
{
  _height = __height;
}


void LocalCartesianParameters::setOrientation( double __orientation )
{
  _orientation = __orientation;
}


double LocalCartesianParameters::longitude() const
{
  return _longitude;
}


double LocalCartesianParameters::latitude() const
{
  return _latitude;
}


double LocalCartesianParameters::height() const
{
  return _height;
}


double LocalCartesianParameters::orientation() const
{
  return _orientation;
}



// CLASSIFICATION: UNCLASSIFIED
