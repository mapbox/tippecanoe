####
# Compiler declarations
####

#Sun SPARCworks
#cc = cc
#CC = CC
#posic = -G -mt -KPIC -DSOLARIS -DSOLARIS_2 -DNOIDENT -DFUNCPROTO -Dsun4sol -D_REENTRANT 
#COMPILER_BOOLS = -DCOMPILER_HAS_NO_BOOLEAN
#COMPILER_ISSUES = $(COMPILER_BOOLS) $(COMPILER_CASTS)

#Gnu
cc = gcc
CC = g++
posic = -shared -fPIC -m64 -pthread -std=gnu++98 -Wno-deprecated  -D_GNU_SOURCE -DREDHAT
COMPILER_ISSUES = $(COMPILER_CASTS)

####
# Support declarations
####

DEBUG = -g

CFLAGS = $(DEBUG)
CCFLAGS = $(CFLAGS) $(COMPILER_ISSUES)

####
# Directories
####

DTCCDIR = ../../CCS/src/dtcc/CoordinateSystems#../../dt_cc
CCSERVICEDIR = ../../CCS/src
SRCDIR = ../../GEOTRANS3/java_gui/geotrans3/jni
JNIDIR = ../../GEOTRANS3/java_gui/geotrans3/jni
JAVADIR = /usr/jdk1.5.0_18#Note: Change this for your system

####
# Libraries
####

LIBMSPDTCC = libMSPdtcc.so
LIBMSPCCS = libMSPCoordinateConversionService.so
LIBJNI = libjnimsp_ccs.so

#LIBS = $(XPCLASSLIBS) $(MOTIFLIBS) $(XLIBS) -lm

####
# Sources
####

DTCCSRCS = \
        threads/CCSThreadMutex.cpp \
        threads/CCSThreadLock.cpp \
        albers/AlbersEqualAreaConic.cpp \
        azeq/AzimuthalEquidistant.cpp \
        bng/BritishNationalGrid.cpp \
        bonne/Bonne.cpp \
        cassini/Cassini.cpp \
        cyleqa/CylindricalEqualArea.cpp \
        datum/Datum.cpp \
        datum/DatumLibraryImplementation.cpp \
        datum/SevenParameterDatum.cpp \
        datum/ThreeParameterDatum.cpp \
        eckert4/Eckert4.cpp \
        eckert6/Eckert6.cpp \
        ellipse/Ellipsoid.cpp \
        ellipse/EllipsoidLibraryImplementation.cpp \
        eqdcyl/EquidistantCylindrical.cpp \
        gars/GARS.cpp \
        geocent/Geocentric.cpp \
        georef/GEOREF.cpp \
        gnomonic/Gnomonic.cpp \
        grinten/VanDerGrinten.cpp \
        lambert/LambertConformalConic.cpp \
        loccart/LocalCartesian.cpp \
        mercator/Mercator.cpp \
        mgrs/MGRS.cpp \
        miller/MillerCylindrical.cpp \
        misc/CoordinateSystem.cpp \
        mollweid/Mollweide.cpp \
        neys/Neys.cpp \
        nzmg/NZMG.cpp \
        omerc/ObliqueMercator.cpp \
        orthogr/Orthographic.cpp \
        polarst/PolarStereographic.cpp \
        polycon/Polyconic.cpp \
        sinusoid/Sinusoidal.cpp \
        stereogr/Stereographic.cpp \
        trcyleqa/TransverseCylindricalEqualArea.cpp \
        tranmerc/TransverseMercator.cpp \
        ups/UPS.cpp \
        usng/USNG.cpp \
        utm/UTM.cpp

DTCC_O = \
        CCSThreadMutex.cpp \
        CCSThreadLock.cpp \
        AlbersEqualAreaConic.cpp \
        AzimuthalEquidistant.cpp \
        BritishNationalGrid.cpp \
        Bonne.cpp \
        Cassini.cpp \
        CylindricalEqualArea.cpp \
        Datum.cpp \
        DatumLibraryImplementation.cpp \
        SevenParameterDatum.cpp \
        ThreeParameterDatum.cpp \
        Eckert4.cpp \
        Eckert6.cpp \
        Ellipsoid.cpp \
        EllipsoidLibraryImplementation.cpp \
        EquidistantCylindrical.cpp \
        GARS.cpp \
        Geocentric.cpp \
        GEOREF.cpp \
        Gnomonic.cpp \
        VanDerGrinten.cpp \
        LambertConformalConic.cpp \
        LocalCartesian.cpp \
        Mercator.cpp \
        MGRS.cpp \
        MillerCylindrical.cpp \
        CoordinateSystem.cpp \
        Mollweide.cpp \
        Neys.cpp \
        NZMG.cpp \
        ObliqueMercator.cpp \
        Orthographic.cpp \
        PolarStereographic.cpp \
        Polyconic.cpp \
        Sinusoidal.cpp \
        Stereographic.cpp \
        TransverseCylindricalEqualArea.cpp \
        TransverseMercator.cpp \
        UPS.cpp \
        USNG.cpp \
        UTM.cpp

CCSSRCS = \
        CoordinateConversion/CoordinateConversionService.cpp

CCSERVICESRCS = \
        dtcc/DatumLibrary.cpp \
        dtcc/EllipsoidLibrary.cpp \
        dtcc/GeoidLibrary.cpp \
        dtcc/egm2008_geoid_grid.cpp \
        dtcc/egm2008_full_grid_package.cpp \
        dtcc/egm2008_aoi_grid_package.cpp \
        dtcc/CoordinateSystemParameters/CoordinateSystemParameters.cpp \
        dtcc/CoordinateSystemParameters/EllipsoidParameters.cpp \
        dtcc/CoordinateSystemParameters/EquidistantCylindricalParameters.cpp \
        dtcc/CoordinateSystemParameters/GeodeticParameters.cpp \
        dtcc/CoordinateSystemParameters/LocalCartesianParameters.cpp \
        dtcc/CoordinateSystemParameters/MapProjection3Parameters.cpp \
        dtcc/CoordinateSystemParameters/MapProjection4Parameters.cpp \
        dtcc/CoordinateSystemParameters/MapProjection5Parameters.cpp \
        dtcc/CoordinateSystemParameters/MapProjection6Parameters.cpp \
        dtcc/CoordinateSystemParameters/MercatorStandardParallelParameters.cpp \
        dtcc/CoordinateSystemParameters/MercatorScaleFactorParameters.cpp \
        dtcc/CoordinateSystemParameters/NeysParameters.cpp \
        dtcc/CoordinateSystemParameters/ObliqueMercatorParameters.cpp \
        dtcc/CoordinateSystemParameters/PolarStereographicStandardParallelParameters.cpp \
        dtcc/CoordinateSystemParameters/PolarStereographicScaleFactorParameters.cpp \
        dtcc/CoordinateSystemParameters/UTMParameters.cpp \
        dtcc/CoordinateTuples/Accuracy.cpp \
        dtcc/CoordinateTuples/BNGCoordinates.cpp \
        dtcc/CoordinateTuples/CartesianCoordinates.cpp \
        dtcc/CoordinateTuples/CoordinateTuple.cpp \
        dtcc/CoordinateTuples/GARSCoordinates.cpp \
        dtcc/CoordinateTuples/GeodeticCoordinates.cpp \
        dtcc/CoordinateTuples/GEOREFCoordinates.cpp \
        dtcc/CoordinateTuples/MapProjectionCoordinates.cpp \
        dtcc/CoordinateTuples/MGRSorUSNGCoordinates.cpp \
        dtcc/CoordinateTuples/UPSCoordinates.cpp \
        dtcc/CoordinateTuples/UTMCoordinates.cpp \
        dtcc/Exception/ErrorMessages.cpp \
        dtcc/Exception/WarningMessages.cpp

CCS_O = \
        CoordinateConversionService.cpp

CCSERVICE_O = \
        DatumLibrary.cpp \
        EllipsoidLibrary.cpp \
        GeoidLibrary.cpp \
        egm2008_geoid_grid.cpp \
        egm2008_full_grid_package.cpp \
        egm2008_aoi_grid_package.cpp \
        CoordinateSystemParameters.cpp \
        EllipsoidParameters.cpp \
        EquidistantCylindricalParameters.cpp \
        GeodeticParameters.cpp \
        LocalCartesianParameters.cpp \
        MapProjection3Parameters.cpp \
        MapProjection4Parameters.cpp \
        MapProjection5Parameters.cpp \
        MapProjection6Parameters.cpp \
        MercatorStandardParallelParameters.cpp \
        MercatorScaleFactorParameters.cpp \
        NeysParameters.cpp \
        ObliqueMercatorParameters.cpp \
        PolarStereographicStandardParallelParameters.cpp \
        PolarStereographicScaleFactorParameters.cpp \
        UTMParameters.cpp \
        Accuracy.cpp \
        BNGCoordinates.cpp \
        CartesianCoordinates.cpp \
        CoordinateTuple.cpp \
        GARSCoordinates.cpp \
        GeodeticCoordinates.cpp \
        GEOREFCoordinates.cpp \
        MapProjectionCoordinates.cpp \
        MGRSorUSNGCoordinates.cpp \
        UPSCoordinates.cpp \
        UTMCoordinates.cpp \
        ErrorMessages.cpp \
        WarningMessages.cpp

SRCS = \
        strtoval.cpp \
        fiomeths.cpp

JNISRCS = \
        JNICCSObjectTranslator.cpp \
        JNIDatumLibrary.cpp \
        JNIEllipsoidLibrary.cpp \
        JNIFiomeths.cpp \
        ThrowException.cpp \
        JNICoordinateConversionService.cpp

####
# Includes
####

DTCCINCS = \
        -I$(DTCCDIR)/threads \
        -I$(DTCCDIR)/albers \
        -I$(DTCCDIR)/azeq \
        -I$(DTCCDIR)/bonne \
        -I$(DTCCDIR)/bng \
        -I$(DTCCDIR)/cassini \
        -I$(DTCCDIR)/cyleqa \
        -I$(DTCCDIR)/datum \
        -I$(DTCCDIR)/eckert4 \
        -I$(DTCCDIR)/eckert6 \
        -I$(DTCCDIR)/ellipse \
        -I$(DTCCDIR)/eqdcyl \
        -I$(DTCCDIR)/gars \
        -I$(DTCCDIR)/geocent \
        -I$(DTCCDIR)/geoid \
        -I$(DTCCDIR)/georef \
        -I$(DTCCDIR)/gnomonic \
        -I$(DTCCDIR)/grinten \
        -I$(DTCCDIR)/lambert \
        -I$(DTCCDIR)/loccart \
        -I$(DTCCDIR)/mercator \
        -I$(DTCCDIR)/misc \
        -I$(DTCCDIR)/mgrs \
        -I$(DTCCDIR)/miller \
        -I$(DTCCDIR)/mollweid \
        -I$(DTCCDIR)/neys \
        -I$(DTCCDIR)/nzmg \
        -I$(DTCCDIR)/omerc \
        -I$(DTCCDIR)/orthogr \
        -I$(DTCCDIR)/polarst \
        -I$(DTCCDIR)/polycon \
        -I$(DTCCDIR)/sinusoid \
        -I$(DTCCDIR)/stereogr \
        -I$(DTCCDIR)/trcyleqa \
        -I$(DTCCDIR)/tranmerc \
        -I$(DTCCDIR)/ups \
        -I$(DTCCDIR)/usng \
        -I$(DTCCDIR)/utm \
        -I$(DTCCDIR)/webmerc


INCLUDES = \
        -I$(CCSERVICEDIR)/dtcc \
        -I$(CCSERVICEDIR)/CoordinateConversion \
        -I$(CCSERVICEDIR)/dtcc/CoordinateSystemParameters \
        -I$(CCSERVICEDIR)/dtcc/CoordinateTuples \
        -I$(CCSERVICEDIR)/dtcc/Enumerations \
        -I$(CCSERVICEDIR)/dtcc/Exception \
        -I$(SRCDIR)

JNIINCS = \
        -I$(JNIDIR)

JAVAINCS = \
        -I$(JAVADIR)/include \
        -I$(JAVADIR)/include/linux

####
# Targets
####

all:    $(LIBMSPDTCC) $(LIBMSPCCS)

# make libMSPdtcc.so
$(LIBMSPDTCC):   $(DTCCSRCS) $(DTCCSRCS:.cpp=.o) $(CCSERVICESRCS) $(CCSERVICESRCS:.cpp=.o)
	$(CC) -dy -o $(LIBMSPDTCC) $(posic) $(DTCC_O:.cpp=.o) $(CCSERVICE_O:.cpp=.o) -lm

#MSPCCS shared object library
# make libmsp_ccs.so
$(LIBMSPCCS):   $(CCSSRCS) $(CCSSRCS:.cpp=.o)
	$(CC) -dy -o $(LIBMSPCCS) $(posic) $(CCS_O:.cpp=.o) -lm

#JNI shared object library
# make libjnimsp_ccs.so
$(LIBJNI): posic_sources $(JNISRCS) $(JNISRCS:.cpp=.o)
	$(CC) -dy -o $(LIBJNI) $(posic) $(LIBMSPCCS) $(LIBMSPDTCC) $(JNISRCS:.cpp=.o) $(SRCS:.cpp=.o) -lm

####
# Supporting Rules
####

#DT&CC Modules - for MSPCCS shared object library
$(DTCCSRCS):
	ln -s $(DTCCDIR)/$@ .

$(DTCCSRCS:.cpp=.o):$(@:.o=.cpp)
	$(CC) -c $(CFLAGS) $(posic) $(INCLUDES) $(DTCCINCS) $(DTCCDIR)/$(@:.o=.cpp)

#MSPCCS CCService - for MSPCCS shared object library
$(CCSERVICESRCS):
	ln -s $(CCSERVICEDIR)/$(@:.cpp=)/$@ .

$(CCSERVICESRCS:.cpp=.o):$(@:.o=.cpp)
	$(CC) -c $(CFLAGS) $(posic) $(INCLUDES) $(DTCCINCS) $(CCSERVICEDIR)/$(@:.o=.cpp)

#MSPCCS CCService - for MSPCCS shared object library
$(CCSSRCS):
	ln -s $(CCSERVICEDIR)/$(@:.cpp=)/$@ .

$(CCSSRCS:.cpp=.o):$(@:.o=.cpp)
	$(CC) -c $(CFLAGS) $(posic) $(INCLUDES) $(DTCCINCS) $(CCSERVICEDIR)/$(@:.o=.cpp)

#Misc MSPCCS sources
$(SRCS):
	ln -s $(SRCDIR)/$@ .

posic_sources:
	make "P=$(posic)" $(SRCS) $(SRCS:.cpp=.o)

force_rebuild:

$(SRCS:.cpp=.o):$(@:.o=.cpp) force_rebuild
	$(CC) -c $(CFLAGS) $(P) $(INCLUDES) $(@:.o=.cpp)

#JNI sources - for Java version
$(JNISRCS):
	ln -s $(JNIDIR)/$@ .

$(JNISRCS:.cpp=.o):$(@:.o=.cpp)
	$(CC) -c $(CFLAGS) $(posic) $(INCLUDES) $(JNIINCS) $(JAVAINCS) $(@:.o=.cpp)

clean:
	$(RM) *.c *.cpp *.o $(BINARY)
