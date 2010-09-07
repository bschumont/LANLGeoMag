#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <Lgm_CTrans.h>
#include <Lgm_Sgp.h>
#include <Lgm_MagEphemInfo.h>

#define KP_DEFAULT 0

void ComputeLstarVersusPA( long int Date, double ut, Lgm_Vector *u, int nAlpha, double *Alpha, int Quality, Lgm_MagEphemInfo *MagEphemInfo );
void WriteMagEphemHeader( FILE *fp, char *UserName, char *Machine, Lgm_MagEphemInfo *m );
void WriteMagEphemData( FILE *fp, Lgm_MagEphemInfo *m );


/*
 *  Compute position of S/C from TLE (Two Line Elements). 
 *  Then use this to compute Magnetic Ephemerides...
 */

int main( int argc, char *argv[] ){

    long int        StartDate, EndDate;
    double          tsince, JD, StartUT, EndUT;
    double          GpsTime, StartGpsTime, StopGpsTime, GpsInc;
    Lgm_CTrans      *c = Lgm_init_ctrans( 0 );
    Lgm_Vector      Ugsm, Uteme;
    Lgm_DateTime    UTC;
    int             nTLEs;
    char            Line0[100], Line1[100], Line2[100], *ptr;
    char            *InputFile  = "input.txt";
    char            *OutputFile = "output.txt";
    FILE            *fp;


double           Alpha[1000], a;
int              nAlpha, Kp;
Lgm_MagEphemInfo *MagEphemInfo = Lgm_InitMagEphemInfo(0);



    // Settings for Lstar calcs
    MagEphemInfo->LstarQuality   = 2;
    MagEphemInfo->SaveShellLines = TRUE;
    MagEphemInfo->LstarInfo->LSimpleMax     = 10.0;
    MagEphemInfo->LstarInfo->VerbosityLevel = 0;
    MagEphemInfo->LstarInfo->mInfo->VerbosityLevel = 0;

    Kp = 5;
    MagEphemInfo->LstarInfo->mInfo->Bfield        = Lgm_B_edip;
    MagEphemInfo->LstarInfo->mInfo->Bfield        = Lgm_B_cdip;
    MagEphemInfo->LstarInfo->mInfo->Bfield        = Lgm_B_igrf;
    MagEphemInfo->LstarInfo->mInfo->Bfield        = Lgm_B_T89;
    MagEphemInfo->LstarInfo->mInfo->InternalModel = LGM_CDIP;
    MagEphemInfo->LstarInfo->mInfo->InternalModel = LGM_IGRF;
    MagEphemInfo->LstarInfo->mInfo->Kp = ( Kp >= 0 ) ? Kp : KP_DEFAULT;
    if ( MagEphemInfo->LstarInfo->mInfo->Kp > 5 ) MagEphemInfo->LstarInfo->mInfo->Kp = 5;

    // Create array of Pitch Angles to compute
    for (nAlpha=0,a=5.0; a<=90.0; a+=5.0,++nAlpha) {
        Alpha[nAlpha] = a ;
        MagEphemInfo->Alpha[nAlpha] = a;
        printf("Alpha[%d] = %g\n", nAlpha, Alpha[nAlpha]);
    }
nAlpha = 0.0;
    MagEphemInfo->nAlpha = nAlpha;



    /* 
     * Open input file and extract:
     *   1) the 3 TLE lines
     *   2) Start Date and Time
     *   3) End Date and Time
     */
    if ( (fp = fopen( InputFile, "r" )) != NULL ) {
        fgets( Line0, 99, fp );
        fgets( Line1, 99, fp );
        fgets( Line2, 99, fp );
        fscanf( fp, "%ld %lf", &StartDate, &StartUT );
        fscanf( fp, "%ld %lf", &EndDate, &EndUT );
    } else {
        printf( "Couldnt open file %s for reading\n", InputFile );
        exit( 1 );
    }
    fclose( fp );

    /*
     * Remove any extraneous newline and/or linefeeds at the end of the strings.
     * Probably not needed, but TLEs may have non-linux terminating characters...
     */
    if ( (ptr = strstr(Line0, "\n")) != NULL ) *ptr = '\0'; 
    if ( (ptr = strstr(Line0, "\r")) != NULL ) *ptr = '\0';
    if ( (ptr = strstr(Line1, "\n")) != NULL ) *ptr = '\0'; 
    if ( (ptr = strstr(Line1, "\r")) != NULL ) *ptr = '\0';
    if ( (ptr = strstr(Line2, "\n")) != NULL ) *ptr = '\0'; 
    if ( (ptr = strstr(Line2, "\r")) != NULL ) *ptr = '\0';


    /*
     * Alloc some memory for the SgpInfo structure and the TLEs array (here we
     * only have a single element in the array)
     */
    _SgpInfo *s   = (_SgpInfo *)calloc( 1, sizeof(_SgpInfo) );
    _SgpTLE *TLEs = (_SgpTLE *)calloc( 1, sizeof(_SgpTLE) );

    
    /*
     * Read in TLEs from the Line0, Line1 and Line2 strings. nTLEs must be
     * initialized to zero by the user.
     */
    nTLEs = 0;
    LgmSgp_ReadTlesFromStrings( Line0, Line1, Line2, &nTLEs, TLEs, 1 );


    /*
     * All the TLEs have their own epoch times in them. And the propagator
     * (sgp4) uses the "time since (in minutes)". So for a given time of
     * interest, we need to compute the tsince needed. Convert Start/End Dates
     * to Julian dates -- they are easier to loop over contiguously.
     */
    Lgm_Make_UTC( StartDate, StartUT, &UTC, c );
    StartGpsTime = Lgm_UTC_to_GpsSeconds( &UTC, c );


    Lgm_Make_UTC( EndDate, EndUT, &UTC, c );
    StopGpsTime = Lgm_UTC_to_GpsSeconds( &UTC, c );

    GpsInc = 60.0; // seconds

    if ( (fp = fopen( OutputFile, "w" )) == NULL ) {
        printf( "Couldnt open file %s for writing\n", OutputFile );
        exit( 1 );
    }

    // init SGP4
    LgmSgp_SGP4_Init( s, &TLEs[0] );
    printf("%%%s\n", TLEs[0].Line0 );
    printf("%%%s\n", TLEs[0].Line1 );
    printf("%%%s\n", TLEs[0].Line2 );
    fprintf(fp, "%%%s\n", TLEs[0].Line0 );
    fprintf(fp, "%%%s\n", TLEs[0].Line1 );
    fprintf(fp, "%%%s\n", TLEs[0].Line2 );


    /*
     * Open Mag Ephem file for writing
     */
    FILE *fp_MagEphem;
    fp_MagEphem = fopen( "puke.txt", "wb" );
    WriteMagEphemHeader( fp_MagEphem, "mgh", "mithril", MagEphemInfo );

    // loop over specified time range
    for ( GpsTime = StartGpsTime; GpsTime <= StopGpsTime; GpsTime += GpsInc ) {


        // Convert the current GpsTime back to Date/UT etc..
        // Need JD to compute tsince
        Lgm_GpsSeconds_to_UTC( GpsTime, &UTC, c ) ;
        JD = Lgm_JD( UTC.Year, UTC.Month, UTC.Day, UTC.Time, LGM_TIME_SYS_UTC, c );

        // Set up the trans matrices
        Lgm_Set_Coord_Transforms( UTC.Date, UTC.Time, c );
    
        // "time since" in minutes (thats what SGP4 wants)
        tsince = (JD - TLEs[0].JD)*1440.0; 

        // Call SGP4. Coords are in TEME. 
        LgmSgp_SGP4( tsince, s );
        Uteme.x = s->X/WGS84_A; Uteme.y = s->Y/WGS84_A; Uteme.z = s->Z/WGS84_A;

        // Example of converting TEME->GSM coords.
        Lgm_Convert_Coords( &Uteme, &Ugsm, TEME_TO_GSM, c );


        /*
         * Compute L*s, Is, Bms, Footprints, etc...
         * These quantities are stored in the MagEphemInfo Structure
         */
        printf("\n\n\nDate, ut = %ld %g   Ugsm = %g %g %g \n", UTC.Date, UTC.Time, Ugsm.x, Ugsm.y, Ugsm.z );
        ComputeLstarVersusPA( UTC.Date, UTC.Time, &Ugsm, nAlpha, Alpha, MagEphemInfo->LstarQuality, MagEphemInfo );

        WriteMagEphemData( fp_MagEphem, MagEphemInfo );

        long int fd = open("test.dat", O_CREAT|O_WRONLY);
        write( fd, MagEphemInfo, sizeof(*MagEphemInfo) );
        close(fd);


    }
    fclose(fp);
    fclose(fp_MagEphem);

    Lgm_free_ctrans( c );
    free( s );
    free( TLEs );
    Lgm_FreeMagEphemInfo( MagEphemInfo );



    return(0);
}
