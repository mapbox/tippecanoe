// CLASSIFICATION: UNCLASSIFIED

#include "MapProjection5Parameters.h"


using namespace MSP::CCS;


MapProjection5Parameters::MapProjection5Parameters() :
CoordinateSystemParameters( CoordinateType::transverseMercator ),
  _centralMeridian( 0 ),
  _originLatitude( 0 ),
  _scaleFactor( 1.0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


MapProjection5Parameters::MapProjection5Parameters( CoordinateType::Enum _coordinateType ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( 0 ),
  _originLatitude( 0 ),
  _scaleFactor( 1.0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 )
{
}


MapProjection5Parameters::MapProjection5Parameters( CoordinateType::Enum _coordinateType, double __centralMeridian, double __originLatitude, double __scaleFactor, double __falseEasting, double __falseNorthing ) :
  CoordinateSystemParameters( _coordinateType ),
  _centralMeridian( __centralMeridian ),
  _originLatitude( __originLatitude ),
  _scaleFactor( __scaleFactor ),
  _falseEasting( __falseEasting ),
  _falseNorthing( __falseNorthing )
{
}


MapProjection5Parameters::MapProjection5Parameters( const MapProjection5Parameters &p )
{
  _coordinateType = p._coordinateType;

  _centralMeridian = p._centralMeridian;
  _originLatitude = p._originLatitude;
  _scaleFactor = p._scaleFactor;
  _falseEasting = p._falseEasting;
  _falseNorthing = p._falseNorthing;
}


MapProjection5Parameters::~MapProjection5Parameters()
{
  _centralMeridian = 0;
  _originLatitude = 0;
  _scaleFactor = 0;
  _falseEasting = 0;
  _falseNorthing = 0;
}


MapProjection5Parameters& MapProjection5Parameters::operator=( const MapProjection5Parameters &p )
{
  if( this != &p )
  {
    _coordinateType = p._coordinateType;

    _centralMeridian = p._centralMeridian;
    _originLatitude = p._originLatitude;
    _scaleFactor = p._scaleFactor;
    _falseEasting = p._falseEasting;
    _falseNorthing = p._falseNorthing;
  }

  return *this;
}


void MapProjection5Parameters::setCentralMeridian( double __centralMeridian )
{
  _centralMeridian = __centralMeridian;
}


void MapProjection5Parameters::setOriginLatitude( double __originLatitude )
{
  _originLatitude = __originLatitude;
}


void MapProjection5Parameters::setScaleFactor( double __scaleFactor )
{
  _scaleFactor = __scaleFactor;
}


void MapProjection5Parameters::setFalseEasting( double __falseEasting )
{
  _falseEasting = __falseEasting;
}


void MapProjection5Parameters::setFalseNorthing( double __falseNorthing )
{
  _falseNorthing = __falseNorthing;
}


double MapProjection5Parameters::centralMeridian() const
{
  return _centralMeridian;
}


double MapProjection5Parameters::originLatitude() const
{
  return _originLatitude;
}


double MapProjection5Parameters::scaleFactor() const
{
  return _scaleFactor;
}


double MapProjection5Parameters::falseEasting() const
{
  return _falseEasting;
}


double MapProjection5Parameters::falseNorthing() const
{
  return _falseNorthing;
}



// CLASSIFICATION: UNCLASSIFIED
