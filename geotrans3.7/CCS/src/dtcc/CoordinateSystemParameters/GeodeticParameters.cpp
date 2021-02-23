// CLASSIFICATION: UNCLASSIFIED

#include "stdio.h"
#include "GeodeticParameters.h"


using namespace MSP::CCS;


GeodeticParameters::GeodeticParameters() :
  CoordinateSystemParameters( CoordinateType::geodetic ),
  _heightType( HeightType::noHeight )
{
}


GeodeticParameters::GeodeticParameters( CoordinateType::Enum _coordinateType ) :
  CoordinateSystemParameters( _coordinateType ),
  _heightType( HeightType::noHeight )
{
}


GeodeticParameters::GeodeticParameters( CoordinateType::Enum _coordinateType, HeightType::Enum __heightType ) :
  CoordinateSystemParameters( _coordinateType ),
  _heightType( __heightType )
{
}


GeodeticParameters::GeodeticParameters( const GeodeticParameters &gp )
{
  _coordinateType = gp._coordinateType;

  _heightType = gp._heightType;
}


GeodeticParameters::~GeodeticParameters()
{
}


GeodeticParameters& GeodeticParameters::operator=( const GeodeticParameters &gp )
{
  if( this != &gp )
  {
    _coordinateType = gp._coordinateType;

    _heightType = gp._heightType;
  }

  return *this;
}


void GeodeticParameters::setHeightType( HeightType::Enum __heightType )
{
	_heightType = __heightType;
}


HeightType::Enum GeodeticParameters::heightType() const
{
  return _heightType;
}



// CLASSIFICATION: UNCLASSIFIED
