// CLASSIFICATION: UNCLASSIFIED

#include <string.h>
#include "MGRSorUSNGCoordinates.h"
#include "Precision.h"


using namespace MSP::CCS;


MGRSorUSNGCoordinates::MGRSorUSNGCoordinates() :
  CoordinateTuple( CoordinateType::militaryGridReferenceSystem ),
  _precision( Precision::tenthOfSecond )
{
  strncpy( _MGRSString, "31NEA0000000000", 20 );
}


MGRSorUSNGCoordinates::MGRSorUSNGCoordinates( CoordinateType::Enum _coordinateType ) :
  CoordinateTuple( _coordinateType ),
  _precision( Precision::tenthOfSecond )
{
  strncpy( _MGRSString, "31NEA0000000000", 20 );
}


MGRSorUSNGCoordinates::MGRSorUSNGCoordinates( CoordinateType::Enum _coordinateType, const char* __MGRSString ) :
  CoordinateTuple( _coordinateType ),
  _precision( Precision::tenthOfSecond )
{
  strncpy( _MGRSString, __MGRSString, 20 );
  _MGRSString[20] = '\0';
}


MGRSorUSNGCoordinates::MGRSorUSNGCoordinates( CoordinateType::Enum _coordinateType, const char* __MGRSString, Precision::Enum __precision ) :
  CoordinateTuple( _coordinateType ),
  _precision( __precision )
{
  strncpy( _MGRSString, __MGRSString, 20 );
  _MGRSString[20] = '\0';
}


MGRSorUSNGCoordinates::MGRSorUSNGCoordinates( CoordinateType::Enum _coordinateType, const char* __warningMessage, const char* __MGRSString, Precision::Enum __precision ) :
  CoordinateTuple( _coordinateType ),
  _precision( __precision )
{
  strncpy( _MGRSString, __MGRSString, 20 );
  _MGRSString[20] = '\0';

  int length = strlen( __warningMessage );
  strncpy( _warningMessage, __warningMessage, length );
  _warningMessage[ length ] = '\0';
}


MGRSorUSNGCoordinates::MGRSorUSNGCoordinates( const MGRSorUSNGCoordinates &c )
{
  _coordinateType = c._coordinateType;

  strncpy( _MGRSString, c._MGRSString, 20 );
  _MGRSString[20] = '\0';

  _precision = c._precision;

  int length = strlen( c._warningMessage );
  strncpy( _warningMessage, c._warningMessage, length );
  _warningMessage[ length ] = '\0';
}


MGRSorUSNGCoordinates::~MGRSorUSNGCoordinates()
{
}


MGRSorUSNGCoordinates& MGRSorUSNGCoordinates::operator=( const MGRSorUSNGCoordinates &c )
{
  if( this != &c )
  {
    _coordinateType = c._coordinateType;

    strncpy( _MGRSString, c._MGRSString, 20 );
    _MGRSString[20] = '\0';

    _precision = c._precision;

    int length = strlen( c._warningMessage );
    strncpy( _warningMessage, c._warningMessage, length );
    _warningMessage[ length ] = '\0';
  }

  return *this;
}


void MGRSorUSNGCoordinates::set( char __MGRSString[21] )
{
  strncpy( _MGRSString, __MGRSString, 20 );
  _MGRSString[20] = '\0';
}


char* MGRSorUSNGCoordinates::MGRSString()
{
  return _MGRSString;
}


Precision::Enum MGRSorUSNGCoordinates::precision() const
{
   Precision::Enum temp_precision = _precision;

   if (temp_precision < 0)
      temp_precision = Precision::degree;
   if (temp_precision > 5)
      temp_precision = Precision::tenthOfSecond;

  return temp_precision;
}

// CLASSIFICATION: UNCLASSIFIED
