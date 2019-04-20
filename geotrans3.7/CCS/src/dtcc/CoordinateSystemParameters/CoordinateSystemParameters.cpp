// CLASSIFICATION: UNCLASSIFIED

#include "CoordinateSystemParameters.h"
#include "CoordinateType.h"


using namespace MSP::CCS;


CoordinateSystemParameters::CoordinateSystemParameters() :
_coordinateType( CoordinateType::geodetic )
{
}

CoordinateSystemParameters::CoordinateSystemParameters( MSP::CCS::CoordinateType::Enum __coordinateType ) :
  _coordinateType( __coordinateType )
{
}


CoordinateSystemParameters::CoordinateSystemParameters( const CoordinateSystemParameters &csp )
{
  _coordinateType = csp._coordinateType;
}


CoordinateSystemParameters::~CoordinateSystemParameters()
{
}


CoordinateSystemParameters& CoordinateSystemParameters::operator=( const CoordinateSystemParameters &csp )
{
  if( this != &csp )
  {
    _coordinateType = csp._coordinateType;
  }

  return *this;
}


void CoordinateSystemParameters::setCoordinateType( MSP::CCS::CoordinateType::Enum __coordinateType )
{
  _coordinateType = __coordinateType;
}


CoordinateType::Enum CoordinateSystemParameters::coordinateType() const
{
  return _coordinateType;
}

// CLASSIFICATION: UNCLASSIFIED
