// CLASSIFICATION: UNCLASSIFIED

#include "EquidistantCylindricalParameters.h"


using namespace MSP::CCS;


EquidistantCylindricalParameters::EquidistantCylindricalParameters() :
CoordinateSystemParameters( CoordinateType::equidistantCylindrical ),
  _centralMeridian( 0 ),
  _standardParallel( 0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


EquidistantCylindricalParameters::EquidistantCylindricalParameters( CoordinateType::Enum _coordinateType ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( 0 ),
  _standardParallel( 0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


EquidistantCylindricalParameters::EquidistantCylindricalParameters( CoordinateType::Enum _coordinateType, double __centralMeridian, double __standardParallel, double __falseEasting, double __falseNorthing ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( __centralMeridian ),
  _standardParallel( __standardParallel ),
  _falseEasting( __falseEasting ),
  _falseNorthing( __falseNorthing )
{
}


EquidistantCylindricalParameters::EquidistantCylindricalParameters( const EquidistantCylindricalParameters &ecp )
{
  _coordinateType = ecp._coordinateType;

  _centralMeridian = ecp._centralMeridian;
  _standardParallel = ecp._standardParallel;
  _falseEasting = ecp._falseEasting;
  _falseNorthing = ecp._falseNorthing;
}


EquidistantCylindricalParameters::~EquidistantCylindricalParameters()
{
  _centralMeridian = 0;
  _standardParallel = 0;
  _falseEasting = 0;
  _falseNorthing = 0;
}


EquidistantCylindricalParameters& EquidistantCylindricalParameters::operator=( const EquidistantCylindricalParameters &ecp )
{
  if( this != &ecp )
  {
    _coordinateType = ecp._coordinateType;

    _centralMeridian = ecp._centralMeridian;
    _standardParallel = ecp._standardParallel;
    _falseEasting = ecp._falseEasting;
    _falseNorthing = ecp._falseNorthing;
  }

  return *this;
}


void EquidistantCylindricalParameters::setCentralMeridian( double __centralMeridian )
{
  _centralMeridian = __centralMeridian;
}


void EquidistantCylindricalParameters::setStandardParallel( double __standardParallel )
{
  _standardParallel = __standardParallel;
}


void EquidistantCylindricalParameters::setFalseEasting( double __falseEasting )
{
  _falseEasting = __falseEasting;
}


void EquidistantCylindricalParameters::setFalseNorthing( double __falseNorthing )
{
  _falseNorthing = __falseNorthing;
}


double EquidistantCylindricalParameters::centralMeridian() const
{
  return _centralMeridian;
}


double EquidistantCylindricalParameters::standardParallel() const
{
  return _standardParallel;
}


double EquidistantCylindricalParameters::falseEasting() const
{
  return _falseEasting;
}


double EquidistantCylindricalParameters::falseNorthing() const
{
  return _falseNorthing;
}



// CLASSIFICATION: UNCLASSIFIED
