      SUBROUTINE SLATB5( PATH, IMAT, N, TYPE, KL, KU, ANORM, MODE,
     $                   CNDNUM, DIST )
*
*  -- LAPACK test routine (version 3.1) --
*     Craig Lucas, University of Manchester / NAG Ltd.
*     October, 2008
*
*     .. Scalar Arguments ..
      REAL               ANORM, CNDNUM
      INTEGER            IMAT, KL, KU, MODE, N
      CHARACTER          DIST, TYPE
      CHARACTER*3        PATH
*     ..
*
*  Purpose
*  =======
*
*  SLATB5 sets parameters for the matrix generator based on the type
*  of matrix to be generated.
*
*  Arguments
*  =========
*
*  PATH    (input) CHARACTER*3
*          The LAPACK path name.
*
*  IMAT    (input) INTEGER
*          An integer key describing which matrix to generate for this
*          path.
*
*  N       (input) INTEGER
*          The number of rows and columns in the matrix to be generated.
*
*  TYPE    (output) CHARACTER*1
*          The type of the matrix to be generated:
*          = 'S':  symmetric matrix
*          = 'P':  symmetric positive (semi)definite matrix
*          = 'N':  nonsymmetric matrix
*
*  KL      (output) INTEGER
*          The lower band width of the matrix to be generated.
*
*  KU      (output) INTEGER
*          The upper band width of the matrix to be generated.
*
*  ANORM   (output) REAL
*          The desired norm of the matrix to be generated.  The diagonal
*          matrix of singular values or eigenvalues is scaled by this
*          value.
*
*  MODE    (output) INTEGER
*          A key indicating how to choose the vector of eigenvalues.
*
*  CNDNUM  (output) REAL
*          The desired condition number.
*
*  DIST    (output) CHARACTER*1
*          The type of distribution to be used by the random number
*          generator.
*
*  =====================================================================
*
*     .. Parameters ..
      REAL               SHRINK, TENTH
      PARAMETER          ( SHRINK = 0.25E0, TENTH = 0.1E+0 )
      REAL               ONE
      PARAMETER          ( ONE = 1.0E+0 )
      REAL               TWO
      PARAMETER          ( TWO = 2.0E+0 )
*     ..
*     .. Local Scalars ..
      REAL               BADC1, BADC2, EPS, LARGE, SMALL
      LOGICAL            FIRST
      CHARACTER*2        C2
*     ..
*     .. External Functions ..
      REAL               SLAMCH
      EXTERNAL           SLAMCH
*     ..
*     .. Intrinsic Functions ..
      INTRINSIC          MAX, SQRT
*     ..
*     .. External Subroutines ..
      EXTERNAL           SLABAD
*     ..
*     .. Save statement ..
      SAVE               EPS, SMALL, LARGE, BADC1, BADC2, FIRST
*     ..
*     .. Data statements ..
      DATA               FIRST / .TRUE. /
*     ..
*     .. Executable Statements ..
*
*     Set some constants for use in the subroutine.
*
      IF( FIRST ) THEN
         FIRST = .FALSE.
         EPS = SLAMCH( 'Precision' )
         BADC2 = TENTH / EPS
         BADC1 = SQRT( BADC2 )
         SMALL = SLAMCH( 'Safe minimum' )
         LARGE = ONE / SMALL
*
*        If it looks like we're on a Cray, take the square root of
*        SMALL and LARGE to avoid overflow and underflow problems.
*
         CALL SLABAD( SMALL, LARGE )
         SMALL = SHRINK*( SMALL / EPS )
         LARGE = ONE / SMALL
      END IF
*
      C2 = PATH( 2: 3 )
*
*     Set some parameters
*
      DIST = 'S'
      MODE = 3
*
*     Set TYPE, the type of matrix to be generated.
*
      TYPE = C2( 1: 1 )
*
*     Set the lower and upper bandwidths.
*
      IF( IMAT.EQ.1 ) THEN
         KL = 0
      ELSE
         KL = MAX( N-1, 0 )
      END IF
      KU = KL
*
*     Set the condition number and norm.etc
*
      IF( IMAT.EQ.3 ) THEN
         CNDNUM = 1.0E4
         MODE = 2
      ELSE IF( IMAT.EQ.4 ) THEN
         CNDNUM = 1.0E4
         MODE = 1
      ELSE IF( IMAT.EQ.5 ) THEN
         CNDNUM = 1.0E4
         MODE = 3
      ELSE IF( IMAT.EQ.6 ) THEN
         CNDNUM = BADC1
      ELSE IF( IMAT.EQ.7 ) THEN
         CNDNUM = BADC2
      ELSE
         CNDNUM = TWO
      END IF
*
      IF( IMAT.EQ.8 ) THEN
         ANORM = SMALL
      ELSE IF( IMAT.EQ.9 ) THEN
         ANORM = LARGE
      ELSE
         ANORM = ONE
      END IF
*
      IF( N.LE.1 )
     $   CNDNUM = ONE
*
      RETURN
*
*     End of SLATB5
*
      END
