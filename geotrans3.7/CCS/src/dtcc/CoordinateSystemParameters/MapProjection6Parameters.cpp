// CLASSIFICATION: UNCLASSIFIED

#include "MapProjection6Parameters.h"


using namespace MSP::CCS;


MapProjection6Parameters::MapProjection6Parameters() :
CoordinateSystemParameters( CoordinateType::albersEqualAreaConic ),
  _centralMeridian( 0 ),
  _originLatitude( 0),
  _standardParallel1( 0 ),
  _standardParallel2( 0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


MapProjection6Parameters::MapProjection6Parameters( CoordinateType::Enum _coordinateType ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( 0 ),
  _originLatitude( 0),
  _standardParallel1( 0 ),
  _standardParallel2( 0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


MapProjection6Parameters::MapProjection6Parameters( CoordinateType::Enum _coordinateType, double __centralMeridian, double __originLatitude, double __standardParallel1, double __standardParallel2, double __falseEasting, double __falseNorthing ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( __centralMeridian ),
  _originLatitude( __originLatitude ),
  _standardParallel1( __standardParallel1 ),
  _standardParallel2( __standardParallel2 ),
  _falseEasting( __falseEasting ),
  _falseNorthing( __falseNorthing )
{
}


MapProjection6Parameters::MapProjection6Parameters( const MapProjection6Parameters &p )
{
  _coordinateType = p._coordinateType;

  _centralMeridian = p._centralMeridian;
  _originLatitude = p._originLatitude;
  _standardParallel1 = p._standardParallel1;
  _standardParallel2 = p._standardParallel2;
  _falseEasting = p._falseEasting;
  _falseNorthing = p._falseNorthing;
}


MapProjection6Parameters::~MapProjection6Parameters()
{
  _centralMeridian = 0;
  _originLatitude = 0;
  _standardParallel1 = 0;
  _standardParallel2 = 0;
  _falseEasting = 0;
  _falseNorthing = 0;
}


MapProjection6Parameters& MapProjection6Parameters::operator=( const MapProjection6Parameters &p )
{
  if( this != &p )
  {
    _coordinateType = p._coordinateType;

    _centralMeridian = p._centralMeridian;
    _originLatitude = p._originLatitude;
    _standardParallel1 = p._standardParallel1;
    _standardParallel2 = p._standardParallel2;
    _falseEasting = p._falseEasting;
    _falseNorthing = p._falseNorthing;
  }

  return *this;
}


void MapProjection6Parameters::setCentralMeridian( double __centralMeridian )
{
  _centralMeridian = __centralMeridian;
}


void MapProjection6Parameters::setOriginLatitude( double __originLatitude )
{
  _originLatitude = __originLatitude;
}


void MapProjection6Parameters::setStandardParallel1( double __standardParallel1 )
{
  _standardParallel1 = __standardParallel1;
}


void MapProjection6Parameters::setStandardParallel2( double __standardParallel2 )
{
  _standardParallel2 = __standardParallel2;
}


void MapProjection6Parameters::setFalseEasting( double __falseEasting )
{
  _falseEasting = __falseEasting;
}


void MapProjection6Parameters::setFalseNorthing( double __falseNorthing )
{
  _falseNorthing = __falseNorthing;
}


double MapProjection6Parameters::centralMeridian() const
{
  return _centralMeridian;
}


double MapProjection6Parameters::originLatitude() const
{
  return _originLatitude;
}


double MapProjection6Parameters::standardParallel1() const
{
  return _standardParallel1;
}


double MapProjection6Parameters::standardParallel2() const
{
  return _standardParallel2;
}


double MapProjection6Parameters::falseEasting() const
{
  return _falseEasting;
}


double MapProjection6Parameters::falseNorthing() const
{
  return _falseNorthing;
}


// CLASSIFICATION: UNCLASSIFIED
