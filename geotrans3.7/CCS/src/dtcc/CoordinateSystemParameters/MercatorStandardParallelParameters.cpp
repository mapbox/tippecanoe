// CLASSIFICATION: UNCLASSIFIED

#include "MercatorStandardParallelParameters.h"


using namespace MSP::CCS;


MercatorStandardParallelParameters::MercatorStandardParallelParameters() :
  CoordinateSystemParameters( CoordinateType::mercatorStandardParallel ),
  _centralMeridian( 0 ),
  _standardParallel( 0 ),
  _scaleFactor( 1.0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


MercatorStandardParallelParameters::MercatorStandardParallelParameters( CoordinateType::Enum _coordinateType ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( 0 ),
  _standardParallel( 0 ),
  _scaleFactor( 1.0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


MercatorStandardParallelParameters::MercatorStandardParallelParameters( CoordinateType::Enum _coordinateType, double __centralMeridian, double __standardParallel, double __scaleFactor, double __falseEasting, double __falseNorthing ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( __centralMeridian ),
  _standardParallel( __standardParallel ),
  _scaleFactor( __scaleFactor ),
  _falseEasting( __falseEasting ),
  _falseNorthing( __falseNorthing )
{
}


MercatorStandardParallelParameters::MercatorStandardParallelParameters( const MercatorStandardParallelParameters &p )
{
  _coordinateType = p._coordinateType;

  _centralMeridian = p._centralMeridian;
  _standardParallel = p._standardParallel;
  _scaleFactor = p._scaleFactor;
  _falseEasting = p._falseEasting;
  _falseNorthing = p._falseNorthing;
}


MercatorStandardParallelParameters::~MercatorStandardParallelParameters()
{
  _centralMeridian = 0;
  _standardParallel = 0;
  _scaleFactor = 0;
  _falseEasting = 0;
  _falseNorthing = 0;
}


MercatorStandardParallelParameters& MercatorStandardParallelParameters::operator=( const MercatorStandardParallelParameters &p )
{
  if( this != &p )
  {
    _coordinateType = p._coordinateType;

    _centralMeridian = p._centralMeridian;
    _standardParallel = p._standardParallel;
    _scaleFactor = p._scaleFactor;
    _falseEasting = p._falseEasting;
    _falseNorthing = p._falseNorthing;
  }

  return *this;
}


void MercatorStandardParallelParameters::setCentralMeridian( double __centralMeridian )
{
  _centralMeridian = __centralMeridian;
}


void MercatorStandardParallelParameters::setStandardParallel( double __standardParallel )
{
  _standardParallel = __standardParallel;
}


void MercatorStandardParallelParameters::setScaleFactor( double __scaleFactor )
{
  _scaleFactor = __scaleFactor;
}


void MercatorStandardParallelParameters::setFalseEasting( double __falseEasting )
{
  _falseEasting = __falseEasting;
}


void MercatorStandardParallelParameters::setFalseNorthing( double __falseNorthing )
{
  _falseNorthing = __falseNorthing;
}


double MercatorStandardParallelParameters::centralMeridian() const
{
  return _centralMeridian;
}


double MercatorStandardParallelParameters::standardParallel() const
{
  return _standardParallel;
}


double MercatorStandardParallelParameters::scaleFactor() const
{
  return _scaleFactor;
}


double MercatorStandardParallelParameters::falseEasting() const
{
  return _falseEasting;
}


double MercatorStandardParallelParameters::falseNorthing() const
{
  return _falseNorthing;
}



// CLASSIFICATION: UNCLASSIFIED
