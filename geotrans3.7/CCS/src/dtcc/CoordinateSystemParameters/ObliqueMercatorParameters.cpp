// CLASSIFICATION: UNCLASSIFIED

#include "ObliqueMercatorParameters.h"


using namespace MSP::CCS;


ObliqueMercatorParameters::ObliqueMercatorParameters() :
CoordinateSystemParameters( CoordinateType::obliqueMercator ),
  _originLatitude( 0 ),
  _longitude1( 0 ),
  _latitude1( 0 ),
  _longitude2( 0 ),
  _latitude2( 0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 ),
  _scaleFactor( 1.0 )
{
}


ObliqueMercatorParameters::ObliqueMercatorParameters( CoordinateType::Enum _coordinateType ) :
  CoordinateSystemParameters( _coordinateType ),
  _originLatitude( 0 ),
  _longitude1( 0 ),
  _latitude1( 0 ),
  _longitude2( 0 ),
  _latitude2( 0 ),
  _falseEasting( 0 ),
  _falseNorthing( 0 ),
  _scaleFactor( 1.0 )
{
}


ObliqueMercatorParameters::ObliqueMercatorParameters( CoordinateType::Enum _coordinateType, double __originLatitude, double __longitude1, double __latitude1, double __longitude2, double __latitude2, double __falseEasting, double __falseNorthing, double __scaleFactor ) :
  CoordinateSystemParameters( _coordinateType ),
  _originLatitude( __originLatitude ),
  _longitude1( __longitude1 ),
  _latitude1( __latitude1 ),
  _longitude2( __longitude2 ),
  _latitude2( __latitude2 ),
  _falseEasting( __falseEasting ),
  _falseNorthing( __falseNorthing ),
  _scaleFactor( __scaleFactor )
{
}


ObliqueMercatorParameters::ObliqueMercatorParameters( const ObliqueMercatorParameters &p )
{
  _coordinateType = p._coordinateType;

  _originLatitude = p._originLatitude;
  _longitude1 = p._longitude1;
  _latitude1 = p._latitude1;
  _longitude2 = p._longitude2;
  _latitude2 = p._latitude2;
  _falseEasting = p._falseEasting;
  _falseNorthing = p._falseNorthing;
  _scaleFactor = p._scaleFactor;
}


ObliqueMercatorParameters::~ObliqueMercatorParameters()
{
  _originLatitude = 0;
  _longitude1 = 0;
  _latitude1 = 0;
  _longitude2 = 0;
  _latitude2 = 0;
  _falseEasting = 0;
  _falseNorthing = 0;
  _scaleFactor = 1.0;
}


ObliqueMercatorParameters& ObliqueMercatorParameters::operator=( const ObliqueMercatorParameters &p )
{
  if( this != &p )
  {
    _coordinateType = p._coordinateType;

    _originLatitude = p._originLatitude;
    _longitude1 = p._longitude1;
    _latitude1 = p._latitude1;
    _longitude2 = p._longitude2;
    _latitude2 = p._latitude2;
    _falseEasting = p._falseEasting;
    _falseNorthing = p._falseNorthing;
    _scaleFactor = p._scaleFactor;
  }

  return *this;
}


void ObliqueMercatorParameters::setOriginLatitude( double __originLatitude )
{
  _originLatitude = __originLatitude;
}


void ObliqueMercatorParameters::setLongitude1( double __longitude1 )
{
  _longitude1 = __longitude1;
}


void ObliqueMercatorParameters::setLatitude1( double __latitude1 )
{
  _latitude1 = __latitude1;
}


void ObliqueMercatorParameters::setLongitude2( double __longitude2 )
{
  _longitude2 = __longitude2;
}


void ObliqueMercatorParameters::setLatitude2( double __latitude2 )
{
  _latitude2 = __latitude2;
}


void ObliqueMercatorParameters::setFalseEasting( double __falseEasting )
{
  _falseEasting = __falseEasting;
}


void ObliqueMercatorParameters::setFalseNorthing( double __falseNorthing )
{
  _falseNorthing = __falseNorthing;
}


void ObliqueMercatorParameters::setScaleFactor( double __scaleFactor )
{
  _scaleFactor = __scaleFactor;
}


double ObliqueMercatorParameters::originLatitude() const
{
  return _originLatitude;
}


double ObliqueMercatorParameters::longitude1() const
{
  return _longitude1;
}


double ObliqueMercatorParameters::latitude1() const
{
  return _latitude1;
}


double ObliqueMercatorParameters::longitude2() const
{
  return _longitude2;
}


double ObliqueMercatorParameters::latitude2() const
{
  return _latitude2;
}


double ObliqueMercatorParameters::falseEasting() const
{
  return _falseEasting;
}


double ObliqueMercatorParameters::falseNorthing() const
{
  return _falseNorthing;
}


double ObliqueMercatorParameters::scaleFactor() const
{
  return _scaleFactor;
}


// CLASSIFICATION: UNCLASSIFIED
