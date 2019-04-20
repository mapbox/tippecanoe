// CLASSIFICATION: UNCLASSIFIED

#include "MapProjection4Parameters.h"


using namespace MSP::CCS;


MapProjection4Parameters::MapProjection4Parameters() :
CoordinateSystemParameters( CoordinateType::bonne ),
  _centralMeridian( 0 ),
  _originLatitude( 0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


MapProjection4Parameters::MapProjection4Parameters( CoordinateType::Enum _coordinateType ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( 0 ),
  _originLatitude( 0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


MapProjection4Parameters::MapProjection4Parameters( CoordinateType::Enum _coordinateType, double __centralMeridian, double __originLatitude, double __falseEasting, double __falseNorthing ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( __centralMeridian ),
  _originLatitude( __originLatitude ),
  _falseEasting( __falseEasting ),
  _falseNorthing( __falseNorthing )
{
}


MapProjection4Parameters::MapProjection4Parameters( const MapProjection4Parameters &p )
{
  _coordinateType = p._coordinateType;

  _centralMeridian = p._centralMeridian;
  _originLatitude = p._originLatitude;
  _falseEasting = p._falseEasting;
  _falseNorthing = p._falseNorthing;
}


MapProjection4Parameters::~MapProjection4Parameters()
{
  _centralMeridian = 0;
  _originLatitude = 0;
  _falseEasting = 0;
  _falseNorthing = 0;
}


MapProjection4Parameters& MapProjection4Parameters::operator=( const MapProjection4Parameters &p )
{
  if( this != &p )
  {
    _coordinateType = p._coordinateType;

    _centralMeridian = p._centralMeridian;
    _originLatitude = p._originLatitude;
    _falseEasting = p._falseEasting;
    _falseNorthing = p._falseNorthing;
  }

  return *this;
}


void MapProjection4Parameters::setCentralMeridian( double __centralMeridian )
{
  _centralMeridian = __centralMeridian;
}


void MapProjection4Parameters::setOriginLatitude( double __originLatitude )
{
  _originLatitude = __originLatitude;
}


void MapProjection4Parameters::setFalseEasting( double __falseEasting )
{
  _falseEasting = __falseEasting;
}


void MapProjection4Parameters::setFalseNorthing( double __falseNorthing )
{
  _falseNorthing = __falseNorthing;
}


double MapProjection4Parameters::centralMeridian() const
{
  return _centralMeridian;
}


double MapProjection4Parameters::originLatitude() const
{
  return _originLatitude;
}


double MapProjection4Parameters::falseEasting() const
{
  return _falseEasting;
}


double MapProjection4Parameters::falseNorthing() const
{
  return _falseNorthing;
}



// CLASSIFICATION: UNCLASSIFIED
