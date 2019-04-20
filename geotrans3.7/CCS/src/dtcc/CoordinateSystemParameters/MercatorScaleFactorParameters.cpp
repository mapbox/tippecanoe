// CLASSIFICATION: UNCLASSIFIED

#include "MercatorScaleFactorParameters.h"


using namespace MSP::CCS;


MercatorScaleFactorParameters::MercatorScaleFactorParameters() :
  CoordinateSystemParameters( CoordinateType::mercatorScaleFactor ),
  _centralMeridian( 0 ),
  _scaleFactor( 1.0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


MercatorScaleFactorParameters::MercatorScaleFactorParameters( CoordinateType::Enum _coordinateType ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( 0 ),
  _scaleFactor( 1.0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


MercatorScaleFactorParameters::MercatorScaleFactorParameters( CoordinateType::Enum _coordinateType, double __centralMeridian, double __scaleFactor, double __falseEasting, double __falseNorthing ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( __centralMeridian ),
  _scaleFactor( __scaleFactor ),
  _falseEasting( __falseEasting ),
  _falseNorthing( __falseNorthing )
{
}


MercatorScaleFactorParameters::MercatorScaleFactorParameters( const MercatorScaleFactorParameters &p )
{
  _coordinateType = p._coordinateType;

  _centralMeridian = p._centralMeridian;
  _scaleFactor = p._scaleFactor;
  _falseEasting = p._falseEasting;
  _falseNorthing = p._falseNorthing;
}


MercatorScaleFactorParameters::~MercatorScaleFactorParameters()
{
  _centralMeridian = 0;
  _scaleFactor = 0;
  _falseEasting = 0;
  _falseNorthing = 0;
}


MercatorScaleFactorParameters& MercatorScaleFactorParameters::operator=( const MercatorScaleFactorParameters &p )
{
  if( this != &p )
  {
    _coordinateType = p._coordinateType;

    _centralMeridian = p._centralMeridian;
    _scaleFactor = p._scaleFactor;
    _falseEasting = p._falseEasting;
    _falseNorthing = p._falseNorthing;
  }

  return *this;
}


void MercatorScaleFactorParameters::setCentralMeridian( double __centralMeridian )
{
  _centralMeridian = __centralMeridian;
}


void MercatorScaleFactorParameters::setScaleFactor( double __scaleFactor )
{
  _scaleFactor = __scaleFactor;
}


void MercatorScaleFactorParameters::setFalseEasting( double __falseEasting )
{
  _falseEasting = __falseEasting;
}


void MercatorScaleFactorParameters::setFalseNorthing( double __falseNorthing )
{
  _falseNorthing = __falseNorthing;
}


double MercatorScaleFactorParameters::centralMeridian() const
{
  return _centralMeridian;
}


double MercatorScaleFactorParameters::scaleFactor() const
{
  return _scaleFactor;
}


double MercatorScaleFactorParameters::falseEasting() const
{
  return _falseEasting;
}


double MercatorScaleFactorParameters::falseNorthing() const
{
  return _falseNorthing;
}



// CLASSIFICATION: UNCLASSIFIED
