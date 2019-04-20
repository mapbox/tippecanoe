// CLASSIFICATION: UNCLASSIFIED

#include "PolarStereographicStandardParallelParameters.h"


using namespace MSP::CCS;


PolarStereographicStandardParallelParameters::PolarStereographicStandardParallelParameters() :
CoordinateSystemParameters( CoordinateType::polarStereographicStandardParallel ),
  _centralMeridian( 0 ),
  _standardParallel( 0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


PolarStereographicStandardParallelParameters::PolarStereographicStandardParallelParameters( CoordinateType::Enum _coordinateType ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( 0 ),
  _standardParallel( 0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


PolarStereographicStandardParallelParameters::PolarStereographicStandardParallelParameters( CoordinateType::Enum _coordinateType, double __centralMeridian, double __standardParallel, double __falseEasting, double __falseNorthing ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( __centralMeridian ),
  _standardParallel( __standardParallel ),
  _falseEasting( __falseEasting ),
  _falseNorthing( __falseNorthing )
{
}


PolarStereographicStandardParallelParameters::PolarStereographicStandardParallelParameters( const PolarStereographicStandardParallelParameters &p )
{
  _coordinateType = p._coordinateType;

  _centralMeridian = p._centralMeridian;
  _standardParallel = p._standardParallel;
  _falseEasting = p._falseEasting;
  _falseNorthing = p._falseNorthing;
}


PolarStereographicStandardParallelParameters::~PolarStereographicStandardParallelParameters()
{
  _centralMeridian = 0;
  _standardParallel = 0;
  _falseEasting = 0;
  _falseNorthing = 0;
}


PolarStereographicStandardParallelParameters& PolarStereographicStandardParallelParameters::operator=( const PolarStereographicStandardParallelParameters &p )
{
  if( this != &p )
  {
    _coordinateType = p._coordinateType;

    _centralMeridian = p._centralMeridian;
    _standardParallel = p._standardParallel;
    _falseEasting = p._falseEasting;
    _falseNorthing = p._falseNorthing;
  }

  return *this;
}


void PolarStereographicStandardParallelParameters::setCentralMeridian( double __centralMeridian )
{
  _centralMeridian = __centralMeridian;
}


void PolarStereographicStandardParallelParameters::setStandardParallel( double __standardParallel )
{
  _standardParallel = __standardParallel;
}


void PolarStereographicStandardParallelParameters::setFalseEasting( double __falseEasting )
{
  _falseEasting = __falseEasting;
}


void PolarStereographicStandardParallelParameters::setFalseNorthing( double __falseNorthing )
{
  _falseNorthing = __falseNorthing;
}


double PolarStereographicStandardParallelParameters::centralMeridian() const
{
  return _centralMeridian;
}


double PolarStereographicStandardParallelParameters::standardParallel() const
{
  return _standardParallel;
}


double PolarStereographicStandardParallelParameters::falseEasting() const
{
  return _falseEasting;
}


double PolarStereographicStandardParallelParameters::falseNorthing() const
{
  return _falseNorthing;
}



// CLASSIFICATION: UNCLASSIFIED
