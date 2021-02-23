// CLASSIFICATION: UNCLASSIFIED

#ifndef CoordinateConversionException_H
#define CoordinateConversionException_H

/*
 * CoordinateConversionException.h
 *
 * Created on April 15, 2008, 3:12 PM
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

#include <string.h>
#include "DtccApi.h"

/**
 * Handles exceptions thrown by the ccs. 
 * 
 * @author comstam
 */
namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API CoordinateConversionException
    {
      /**
       * Creates a new instance of CoordinateConversionException
       */
      public:
      
        CoordinateConversionException( const char* __message )
        {
          strcpy( _message, __message );
          _message[strlen( __message )] = '\0';
        }

        CoordinateConversionException( 
           const char* __directionStr,
           const char* __coordinateSystemName,
           const char* __separatorStr,
           const char* __message )
        {
          strcpy( _message, __directionStr );
          strcat( _message, __coordinateSystemName );
          strcat( _message, __separatorStr );
          strcat( _message, __message );
          _message[strlen( _message )] = '\0';
        }

        virtual ~CoordinateConversionException() {}


        char* getMessage()
        {
          return _message;
        }

     private:

        char _message[2000];
    };
  }
}

#endif 

// CLASSIFICATION: UNCLASSIFIED
