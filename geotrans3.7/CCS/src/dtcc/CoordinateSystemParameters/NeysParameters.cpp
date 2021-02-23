// CLASSIFICATION: UNCLASSIFIED

#include "NeysParameters.h"


using namespace MSP::CCS;


NeysParameters::NeysParameters() :
CoordinateSystemParameters( CoordinateType::neys ),
  _centralMeridian( 0 ),
  _originLatitude( 0 ),
  _standardParallel1( 0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


NeysParameters::NeysParameters( CoordinateType::Enum _coordinateType ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( 0 ),
  _originLatitude( 0 ),
  _standardParallel1( 0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


NeysParameters::NeysParameters( CoordinateType::Enum _coordinateType, double __centralMeridian, double __originLatitude, double __standardParallel1, double __falseEasting, double __falseNorthing ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( __centralMeridian ),
  _originLatitude( __originLatitude ),
  _standardParallel1( __standardParallel1 ),
  _falseEasting( __falseEasting ),
  _falseNorthing( __falseNorthing )
{
}


NeysParameters::NeysParameters( const NeysParameters &p )
{
  _coordinateType = p._coordinateType;

  _centralMeridian = p._centralMeridian;
  _originLatitude = p._originLatitude;
  _standardParallel1 = p._standardParallel1;
  _falseEasting = p._falseEasting;
  _falseNorthing = p._falseNorthing;
}


NeysParameters::~NeysParameters()
{
  _centralMeridian = 0;
  _originLatitude = 0;
  _standardParallel1 = 0;
  _falseEasting = 0;
  _falseNorthing = 0;
}


NeysParameters& NeysParameters::operator=( const NeysParameters &p )
{
  if( this != &p )
  {
    _coordinateType = p._coordinateType;

    _centralMeridian = p._centralMeridian;
    _originLatitude = p._originLatitude;
    _standardParallel1 = p._standardParallel1;
    _falseEasting = p._falseEasting;
    _falseNorthing = p._falseNorthing;
  }

  return *this;
}


void NeysParameters::setCentralMeridian( double __centralMeridian )
{
  _centralMeridian = __centralMeridian;
}


void NeysParameters::setOriginLatitude( double __originLatitude )
{
  _originLatitude = __originLatitude;
}


void NeysParameters::setStandardParallel1( double __standardParallel1 )
{
  _standardParallel1 = __standardParallel1;
}


void NeysParameters::setFalseEasting( double __falseEasting )
{
  _falseEasting = __falseEasting;
}


void NeysParameters::setFalseNorthing( double __falseNorthing )
{
  _falseNorthing = __falseNorthing;
}


double NeysParameters::centralMeridian() const
{
  return _centralMeridian;
}


double NeysParameters::originLatitude() const
{
  return _originLatitude;
}


double NeysParameters::standardParallel1() const
{
  return _standardParallel1;
}


double NeysParameters::falseEasting() const
{
  return _falseEasting;
}


double NeysParameters::falseNorthing() const
{
  return _falseNorthing;
}


// CLASSIFICATION: UNCLASSIFIED
