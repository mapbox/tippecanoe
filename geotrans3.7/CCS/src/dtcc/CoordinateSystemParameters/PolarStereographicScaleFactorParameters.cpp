// CLASSIFICATION: UNCLASSIFIED

#include "PolarStereographicScaleFactorParameters.h"


using namespace MSP::CCS;


PolarStereographicScaleFactorParameters::PolarStereographicScaleFactorParameters() :
CoordinateSystemParameters( CoordinateType::polarStereographicScaleFactor ),
  _centralMeridian( 0 ),
  _scaleFactor( 0 ),
  _hemisphere( 'N' ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


PolarStereographicScaleFactorParameters::PolarStereographicScaleFactorParameters( CoordinateType::Enum _coordinateType ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( 0 ),
  _scaleFactor( 0 ),
  _hemisphere( 'N' ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


PolarStereographicScaleFactorParameters::PolarStereographicScaleFactorParameters( CoordinateType::Enum _coordinateType, double __centralMeridian, double __scaleFactor, char __hemisphere, double __falseEasting, double __falseNorthing ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( __centralMeridian ),
  _scaleFactor( __scaleFactor ),
  _hemisphere( __hemisphere ),
  _falseEasting( __falseEasting ),
  _falseNorthing( __falseNorthing )
{
}


PolarStereographicScaleFactorParameters::PolarStereographicScaleFactorParameters( const PolarStereographicScaleFactorParameters &p )
{
  _coordinateType = p._coordinateType;

  _centralMeridian = p._centralMeridian;
  _scaleFactor = p._scaleFactor;
  _hemisphere = p._hemisphere;
  _falseEasting = p._falseEasting;
  _falseNorthing = p._falseNorthing;
}


PolarStereographicScaleFactorParameters::~PolarStereographicScaleFactorParameters()
{
  _centralMeridian = 0;
  _scaleFactor = 0;
  _hemisphere = 'N';
  _falseEasting = 0;
  _falseNorthing = 0;
}


PolarStereographicScaleFactorParameters& PolarStereographicScaleFactorParameters::operator=( const PolarStereographicScaleFactorParameters &p )
{
  if( this != &p )
  {
    _coordinateType = p._coordinateType;

    _centralMeridian = p._centralMeridian;
    _scaleFactor = p._scaleFactor;
    _hemisphere = p._hemisphere;
    _falseEasting = p._falseEasting;
    _falseNorthing = p._falseNorthing;
  }

  return *this;
}


void PolarStereographicScaleFactorParameters::setCentralMeridian( double __centralMeridian )
{
  _centralMeridian = __centralMeridian;
}


void PolarStereographicScaleFactorParameters::setScaleFactor( double __scaleFactor )
{
  _scaleFactor = __scaleFactor;
}


void PolarStereographicScaleFactorParameters::setHemisphere( char __hemisphere )
{
  _hemisphere = __hemisphere;
}


void PolarStereographicScaleFactorParameters::setFalseEasting( double __falseEasting )
{
  _falseEasting = __falseEasting;
}


void PolarStereographicScaleFactorParameters::setFalseNorthing( double __falseNorthing )
{
  _falseNorthing = __falseNorthing;
}


double PolarStereographicScaleFactorParameters::centralMeridian() const
{
  return _centralMeridian;
}


double PolarStereographicScaleFactorParameters::scaleFactor() const
{
  return _scaleFactor;
}


char PolarStereographicScaleFactorParameters::hemisphere() const
{
  return _hemisphere;
}


double PolarStereographicScaleFactorParameters::falseEasting() const
{
  return _falseEasting;
}


double PolarStereographicScaleFactorParameters::falseNorthing() const
{
  return _falseNorthing;
}



// CLASSIFICATION: UNCLASSIFIED
