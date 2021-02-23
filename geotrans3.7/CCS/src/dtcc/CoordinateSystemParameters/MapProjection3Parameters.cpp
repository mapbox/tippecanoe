// CLASSIFICATION: UNCLASSIFIED

#include "MapProjection3Parameters.h"


using namespace MSP::CCS;


MapProjection3Parameters::MapProjection3Parameters() :
  CoordinateSystemParameters( CoordinateType::eckert4 ),
  _centralMeridian( 0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


MapProjection3Parameters::MapProjection3Parameters( CoordinateType::Enum _coordinateType ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( 0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


MapProjection3Parameters::MapProjection3Parameters( CoordinateType::Enum _coordinateType, double __centralMeridian, double __falseEasting, double __falseNorthing ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( __centralMeridian ),
  _falseEasting( __falseEasting ),
  _falseNorthing( __falseNorthing )
{
}


MapProjection3Parameters::MapProjection3Parameters( const MapProjection3Parameters &p )
{
  _coordinateType = p._coordinateType;

  _centralMeridian = p._centralMeridian;
  _falseEasting = p._falseEasting;
  _falseNorthing = p._falseNorthing;
}


MapProjection3Parameters::~MapProjection3Parameters()
{
  _centralMeridian = 0;
  _falseEasting = 0;
  _falseNorthing = 0;
}


MapProjection3Parameters& MapProjection3Parameters::operator=( const MapProjection3Parameters &p )
{
  if( this != &p )
  {
    _coordinateType = p._coordinateType;

    _centralMeridian = p._centralMeridian;
    _falseEasting = p._falseEasting;
    _falseNorthing = p._falseNorthing;
  }

  return *this;
}


void MapProjection3Parameters::setCentralMeridian( double __centralMeridian )
{
  _centralMeridian = __centralMeridian;
}


void MapProjection3Parameters::setFalseEasting( double __falseEasting )
{
  _falseEasting = __falseEasting;
}


void MapProjection3Parameters::setFalseNorthing( double __falseNorthing )
{
  _falseNorthing = __falseNorthing;
}


double MapProjection3Parameters::centralMeridian() const
{
  return _centralMeridian;
}


double MapProjection3Parameters::falseEasting() const
{
  return _falseEasting;
}


double MapProjection3Parameters::falseNorthing() const
{
  return _falseNorthing;
}



// CLASSIFICATION: UNCLASSIFIED
