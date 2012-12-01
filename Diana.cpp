/******************************************/
/*
 Diana.cpp
 Dynamic Interactive Audio and Noise Analyzer
 by Uri Nieto, 2009-12
 
*/
/******************************************/

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <math.h>
#include <iostream>
#include <string>
#include <portaudio.h>

// FFT
#include "fft.h"

// Mutex
#include "Thread.h"

// OpenGL
#ifdef __MACOSX_CORE__
  #include <GLUT/glut.h>
#else
  #include <GL/gl.h>
  #include <GL/glu.h>
  #include <GL/glut.h>
#endif

// Platform-dependent sleep routines.
#if defined( __WINDOWS_ASIO__ ) || defined( __WINDOWS_DS__ )
  #include <windows.h>
  #define SLEEP( milliseconds ) Sleep( (DWORD) milliseconds ) 
#else // Unix variants
  #include <unistd.h>
  #define SLEEP( milliseconds ) usleep( (unsigned long) (milliseconds * 1000.0) )
#endif

using namespace std;


//-----------------------------------------------------------------------------
// global variables and #defines
//-----------------------------------------------------------------------------
#define FORMAT                  paFloat32
#define DNA_BUFFER_SIZE         1024
#define DNA_SCROLL_BUFFER_SIZE  DNA_BUFFER_SIZE*60
#define DNA_FFT_SIZE            ( DNA_BUFFER_SIZE * 2 )
#define DNA_SCALE               1.0
#define SAMPLE                  float
#define DNA_PIE                 3.14159265358979
#define DNA_SRATE               44100
#define INC_VAL_MOUSE           1.0f
#define INC_VAL_KB              .75f
#define MONO                    1

typedef double  MY_TYPE;
typedef char BYTE;   // 8-bit unsigned entity.

SAMPLE g_freq = 440;
SAMPLE g_t = 0;


// width and height of the window
GLsizei g_width = 800;
GLsizei g_height = 600;
GLsizei g_last_width = g_width;
GLsizei g_last_height = g_height;

// global audio vars
SAMPLE g_fft_buffer[DNA_FFT_SIZE];
SAMPLE g_audio_buffer[DNA_BUFFER_SIZE]; // latest mono buffer (possibly preview)
SAMPLE g_stereo_buffer[DNA_BUFFER_SIZE*2]; // current stereo buffer (now playing)
SAMPLE g_back_buffer[DNA_BUFFER_SIZE]; // for lissajous
SAMPLE g_cur_buffer[DNA_BUFFER_SIZE];  // current mono buffer (now playing), for lissajous
GLfloat g_window[DNA_BUFFER_SIZE]; // DFT transform window
GLfloat g_log_positions[DNA_FFT_SIZE/2]; // precompute positions for log spacing
GLint g_buffer_size = DNA_BUFFER_SIZE;
GLint g_fft_size = DNA_FFT_SIZE;
SAMPLE g_buffer[DNA_BUFFER_SIZE];
GLint g_scroll_buffer_size = DNA_SCROLL_BUFFER_SIZE;
SAMPLE g_scroll_buffer[DNA_SCROLL_BUFFER_SIZE];
unsigned int g_channels = MONO;
int g_scroll_reader = 0;
int g_scroll_writer = g_scroll_buffer_size - g_buffer_size;

// for waterfall
struct Pt2D { float x; float y; };
Pt2D ** g_spectrums = NULL;
GLuint g_depth = 48;
GLfloat g_z = 0.0f;
GLboolean g_z_set = FALSE;
GLfloat g_space = .12f; 
GLboolean g_downsample = FALSE;
GLint g_wf = 0; // the index associated with the waterfall
GLboolean * g_draw = NULL; // array of booleans for waterfall

// for time domain waterfall
SAMPLE ** g_waveforms = NULL;
GLfloat g_wf_delay_ratio = 1.0f / 3.0f;
GLuint g_wf_delay = (GLuint)(g_depth * g_wf_delay_ratio + .5f);
GLuint g_wf_index = 0;

// pitch
double g_prev_pitch = 0.0;  // Previous pitch (in Hz)
bool g_same_pitch = false;  // If previous pitch is the same as the current one
double g_pitch_err = 0.3;   // Error in pitch estimation (in Hz)
int g_same_pitch_count = 0; // Count of same number of consecutive pitches

// texture
GLuint g_texture;       // Active Texture
GLuint g_key_texture;   // Midi Keyboard texture
GLuint g_keyC_texture;  // Midi Keyboard texture with C pressed
GLuint g_keyCS_texture; // Midi Keyboard texture with C# pressed  
GLuint g_keyD_texture;  // ...
GLuint g_keyDS_texture;
GLuint g_keyE_texture;
GLuint g_keyF_texture;
GLuint g_keyFS_texture;
GLuint g_keyG_texture;
GLuint g_keyGS_texture;
GLuint g_keyA_texture;
GLuint g_keyAS_texture;
GLuint g_keyB_texture;
int g_texture_count = 0;    // Number of frames with the same texture
int g_texture_reset = 30;  // Number of frames before the texture will be changed
bool g_keyboard_on = false; // If the keyboard is on/off

// gain
GLfloat g_gain = 1.5f;
GLfloat g_time_scale = 1.0f;
GLfloat g_freq_scale = 1.0f;
GLfloat g_lissajous_scale = 0.45f;

// how much to see
GLint g_time_view = 1;
GLint g_freq_view = 2;

// for log scaling
GLdouble g_log_space = 0;
GLdouble g_log_factor = 1;

// Threads Management
GLboolean g_ready = FALSE;
Mutex g_mutex;

// osc port
long g_osc_port = 8888;
GLfloat g_size_x = 2.8;
GLfloat g_size_y = 2;
GLfloat g_size_z = 2;
// fill mode
GLenum g_fillmode = GL_FILL;
// light 0 position
GLfloat g_light0_pos[4] = { 2.0f, 1.2f, 4.0f, 1.0f };
// light 1 parameters
GLfloat g_light1_ambient[] = { .2f, .2f, .2f, 1.0f };
GLfloat g_light1_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat g_light1_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat g_light1_pos[4] = { -2.0f, 0.0f, -4.0f, 1.0f };
// fullscreen
GLboolean g_fullscreen = FALSE;
// draw empty
GLboolean g_drawempty = FALSE;
// do
GLboolean g_fog = TRUE;

// modelview stuff
GLfloat g_angle_y = 0.0f;
GLfloat g_angle_x = 0.0f;
GLfloat g_inc = 0.0f;
GLfloat g_inc_y = 0.0;
GLfloat g_inc_x = 0.0;
GLfloat g_linewidth = 2.0f;
bool g_key_rotate_y = false;
bool g_key_rotate_x =  false;

// rotation increments, set to defaults
GLfloat g_inc_val_mouse = INC_VAL_MOUSE;
GLfloat g_inc_val_kb = INC_VAL_KB;


//-----------------------------------------------------------------------------
// function prototypes
//-----------------------------------------------------------------------------
void idleFunc( );
void displayFunc( );
void reshapeFunc( int width, int height );
void keyboardFunc( unsigned char, int, int );
void specialKey( int key, int x, int y );
void specialUpKey( int key, int x, int y);
void mouseFunc( int button, int state, int x, int y );
void initialize_graphics( );
void initialize_glut(int argc, char *argv[]);
void initialize_audio(PaStream *stream);
void stop_portAudio(PaStream *stream);
void render_CA2D();
inline double map_log_spacing( double ratio, double power );
double compute_log_spacing( int fft_size, double factor );
double pitch_estimation(SAMPLE *x);
void autoCorrelation(SAMPLE *x, int bufferSize);
GLuint LoadTextureRAW( const char * filename, int wrap );
void FreeTexture( GLuint texture );
void drawKeyboard();

struct OutputData {
  FILE *fd;
  unsigned int channels;
};


//-----------------------------------------------------------------------------
// name: help()
// desc: ...
//-----------------------------------------------------------------------------
void help()
{
  fprintf( stderr, "----------------------------------------------------\n" );
  fprintf( stderr, "Diana (1.1)\n" );
  fprintf( stderr, "Dynamic Interactive Audio and Noise Analyzer\n" );
  fprintf( stderr, "by Uri Nieto, 2009-12\n" );
  fprintf( stderr, "New York University -- Stanford University\n" );
  fprintf( stderr, "----------------------------------------------------\n" );
  fprintf( stderr, "'h' - print this help message\n" );
  fprintf( stderr, "'f' - toggle fullscreen\n" );
  fprintf( stderr, "'k' - toggle signal / keyboard mode\n" );
  fprintf( stderr, "'CURSOR ARROWS' - rotate signal view\n" );
  fprintf( stderr, "'q' - quit\n" );
  fprintf( stderr, "----------------------------------------------------\n" );
  fprintf( stderr, "\n" );
}

//-----------------------------------------------------------------------------
// Name: paCallback( )
// Desc: callback from portAudio
//-----------------------------------------------------------------------------
static int paCallback( const void *inputBuffer,
			 void *outputBuffer, unsigned long framesPerBuffer,
			 const PaStreamCallbackTimeInfo* timeInfo,
			 PaStreamCallbackFlags statusFlags, void *userData ) 
{
  SAMPLE * obuffy = (SAMPLE *)outputBuffer;
  SAMPLE * ibuffy = (SAMPLE *)inputBuffer;
  
  // Lock mutex
  g_mutex.lock();

  //Copy the input buffer to our g_buffer
  for( int i = 0; i < framesPerBuffer; i++ )
  {
    g_buffer[i] = ibuffy[i];

  }
  
  // set flag
  g_ready = TRUE;
  // Unlock mutex
  g_mutex.unlock();
  
  return 0;
}

//-----------------------------------------------------------------------------
// Name: initialize_glut( )
// Desc: Initializes Glut with the global vars
//-----------------------------------------------------------------------------
void initialize_glut(int argc, char *argv[]) {
  // initialize GLUT
  glutInit( &argc, argv );
  // double buffer, use rgb color, enable depth buffer
  glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
  // initialize the window size
  glutInitWindowSize( g_width, g_height );
  // set the window postion
  glutInitWindowPosition( 400, 100 );
  // create the window
  glutCreateWindow( "Dynamic Interactive Audio and Noise Analyzer");
  // full screen
  if( g_fullscreen )
    glutFullScreen();
  
  // set the idle function - called when idle
  glutIdleFunc( idleFunc );
  // set the display function - called when redrawing
  glutDisplayFunc( displayFunc );
  // set the reshape function - called when client area changes
  glutReshapeFunc( reshapeFunc );
  // set the keyboard function - called on keyboard events
  glutKeyboardFunc( keyboardFunc );
  // set window's to specialKey callback
  glutSpecialFunc( specialKey );
  // set window's to specialUpKey callback (when the key is up is called)
  glutSpecialUpFunc( specialUpKey );
  // set the mouse function - called on mouse stuff
  glutMouseFunc( mouseFunc );
  
  // do our own initialization
  initialize_graphics( );  
}

//-----------------------------------------------------------------------------
// Name: initialize_audio( RtAudio *dac )
// Desc: Initializes PortAudio with the global vars and the stream
//-----------------------------------------------------------------------------
void initialize_audio(PaStream *stream) {
  
    PaStreamParameters outputParameters;
    PaStreamParameters inputParameters;
    PaError err;

    /* Initialize PortAudio */
    Pa_Initialize();

    /* Set output stream parameters */
    outputParameters.device = Pa_GetDefaultOutputDevice();
    outputParameters.channelCount = g_channels;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = 
    Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    /* Set input stream parameters */
    inputParameters.device = Pa_GetDefaultInputDevice();
    inputParameters.channelCount = g_channels;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = 
    Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    /* Open audio stream */
    err = Pa_OpenStream( &stream,
            &inputParameters,
		    &outputParameters,
	        DNA_SRATE, g_buffer_size, paNoFlag, 
		    paCallback, NULL );

    if (err != paNoError) {
        printf("PortAudio error: open stream: %s\n", Pa_GetErrorText(err));
    }
  
    /* Start audio stream */
    err = Pa_StartStream( stream );
    if (err != paNoError) {
        printf(  "PortAudio error: start stream: %s\n", Pa_GetErrorText(err));
    }
}

void stop_portAudio(PaStream *stream) {
    PaError err;

    /* Stop audio stream */
    err = Pa_StopStream( stream );
    if (err != paNoError) {
        printf(  "PortAudio error: stop stream: %s\n", Pa_GetErrorText(err));
    }
    /* Close audio stream */
    err = Pa_CloseStream(stream);
    if (err != paNoError) {
        printf("PortAudio error: close stream: %s\n", Pa_GetErrorText(err));
    }
    /* Terminate audio stream */
    err = Pa_Terminate();
    if (err != paNoError) {
        printf("PortAudio error: terminate: %s\n", Pa_GetErrorText(err));
    }
}

//-----------------------------------------------------------------------------
// Name: main
// Desc: ...
//-----------------------------------------------------------------------------
int main( int argc, char *argv[] )
{
  // Initialize Glut
  initialize_glut(argc, argv);
  
  // Initialize PortAudio
  PaStream *stream;
  PaError err;
  initialize_audio(stream);
  
  // make the transform window
  hanning( g_window, g_buffer_size );
  
  // Set the scrolling buffer to 0
  memset( g_scroll_buffer, 0, g_scroll_buffer_size * sizeof(SAMPLE) );
  
  // Initialize waterfall:
  g_spectrums = new Pt2D *[g_depth];
  for( int i = 0; i < g_depth; i++ )
  {
    g_spectrums[i] = new Pt2D[DNA_FFT_SIZE];
    memset( g_spectrums[i], 0, sizeof(Pt2D)*DNA_FFT_SIZE );
  }
  g_draw = new GLboolean[g_depth];
  memset( g_draw, 1, sizeof(GLboolean)*g_depth );
  
  // Print help
  help();

  // Wait until <enter> is pressed to stop the process
  glutMainLoop();
  
  // Close Stream before exiting
  stop_portAudio(stream);
  
  // free the texture
  FreeTexture( g_texture );

  return EXIT_SUCCESS;
}

//-----------------------------------------------------------------------------
// Name: idleFunc( )
// Desc: callback from GLUT
//-----------------------------------------------------------------------------
void idleFunc( )
{
  // render the scene
  glutPostRedisplay( );
}

//-----------------------------------------------------------------------------
// Name: keyboardFunc( )
// Desc: key event
//-----------------------------------------------------------------------------
void keyboardFunc( unsigned char key, int x, int y )
{
  //printf("key: %c\n", key);
  switch( key )
  {
    // Print Help
    case 'h':
      help();
      break;
      
    // Show Keyboard
    case 'k':
      g_keyboard_on = !g_keyboard_on;
      printf("[Diana]: Keyboard %s\n", g_keyboard_on ? "ON" : "OFF");
      break;
      
    // Fullscreen
    case 'f':
      if( !g_fullscreen )
      {
        g_last_width = g_width;
        g_last_height = g_height;
        glutFullScreen();
      }
      else
        glutReshapeWindow( g_last_width, g_last_height );
      
      g_fullscreen = !g_fullscreen;
      printf("[Diana]: fullscreen: %s\n", g_fullscreen ? "ON" : "OFF" );
      break;
      
    case 'q':
      exit( 0 );
      break;
  }
  // do a reshape since g_eye_y might have changed
  //reshapeFunc( g_width, g_height );
  glutPostRedisplay( );
}

//-----------------------------------------------------------------------------
// Name: specialUpKey( int key, int x, int y)
// Desc: Callback to know when a special key is pressed
//-----------------------------------------------------------------------------
void specialKey(int key, int x, int y) { 
  // Check which (arrow) key is pressed
  switch(key) {
    case GLUT_KEY_LEFT : // Arrow key left is pressed
      g_key_rotate_y = true;
      g_inc_y = -g_inc_val_kb;
      break;
    case GLUT_KEY_RIGHT :    // Arrow key right is pressed
      g_key_rotate_y = true;
      g_inc_y = g_inc_val_kb;
      break;
    case GLUT_KEY_UP :        // Arrow key up is pressed
      g_key_rotate_x = true;
      g_inc_x = g_inc_val_kb;
      break;
    case GLUT_KEY_DOWN :    // Arrow key down is pressed
      g_key_rotate_x = true;
      g_inc_x = -g_inc_val_kb;
      break;   
  }
}  

//-----------------------------------------------------------------------------
// Name: specialUpKey( int key, int x, int y)
// Desc: Callback to know when a special key is up
//-----------------------------------------------------------------------------
void specialUpKey( int key, int x, int y) {
  // Check which (arrow) key is unpressed
  switch(key) {
    case GLUT_KEY_LEFT : // Arrow key left is unpressed
      printf("[Diana]: LEFT\n");
      g_key_rotate_y = false;
      break;
    case GLUT_KEY_RIGHT :    // Arrow key right is unpressed
      printf("[Diana]: RIGHT\n");
      g_key_rotate_y = false;
      break;
    case GLUT_KEY_UP :        // Arrow key up is unpressed
      printf("[Diana]: UP\n");
      g_key_rotate_x = false;
      break;
    case GLUT_KEY_DOWN :    // Arrow key down is unpressed
      printf("[Diana]: DOWN\n");
      g_key_rotate_x = false;
      break;   
  }
}

//-----------------------------------------------------------------------------
// Name: mouseFunc( )
// Desc: handles mouse stuff
//-----------------------------------------------------------------------------
void mouseFunc( int button, int state, int x, int y )
{
  // Switch Keyboard/Signals View
  if( button == GLUT_LEFT_BUTTON )
  {
    if( state == GLUT_DOWN )
    {}
    else
    {
      g_keyboard_on = !g_keyboard_on;
    }
  }
  else if ( button == GLUT_RIGHT_BUTTON )
  {
    if( state == GLUT_DOWN )
    {}
    else
    {}
  }
  else
    g_inc = 0.0f;
  
  glutPostRedisplay( );
}


//-----------------------------------------------------------------------------
// Name: reshapeFunc( )
// Desc: called when window size changes
//-----------------------------------------------------------------------------
void reshapeFunc( int w, int h )
{
  // save the new window size
  g_width = w; g_height = h;
  // map the view port to the client area
  glViewport( 0, 0, w, h );
  // set the matrix mode to project
  glMatrixMode( GL_PROJECTION );
  // load the identity matrix
  glLoadIdentity( );
  // create the viewing frustum
  //gluPerspective( 45.0, (GLfloat) w / (GLfloat) h, .05, 50.0 );
  gluPerspective( 45.0, (GLfloat) w / (GLfloat) h, 1.0, 1000.0 );
  // set the matrix mode to modelview
  glMatrixMode( GL_MODELVIEW );
  // load the identity matrix
  glLoadIdentity( );
  
  // position the view point
  //  void gluLookAt( GLdouble eyeX,
  //                 GLdouble eyeY,
  //                 GLdouble eyeZ,
  //                 GLdouble centerX,
  //                 GLdouble centerY,
  //                 GLdouble centerZ,
  //                 GLdouble upX,
  //                 GLdouble upY,
  //                 GLdouble upZ )
  
  gluLookAt( 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f );
  /*gluLookAt( 0.0f, 3.5f * sin( 0.0f ), 3.5f * cos( 0.0f ), 
            0.0f, 0.0f, 0.0f, 
            0.0f, 1.0f , 0.0f );*/
   
}


//-----------------------------------------------------------------------------
// Name: initialize_graphics( )
// Desc: sets initial OpenGL states and initializes any application data
//-----------------------------------------------------------------------------
void initialize_graphics()
{
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);					// Black Background
  // set the shading model to 'smooth'
  glShadeModel( GL_SMOOTH );
  // enable depth
  glEnable( GL_DEPTH_TEST );
  // set the front faces of polygons
  glFrontFace( GL_CCW );
  // set fill mode
  glPolygonMode( GL_FRONT_AND_BACK, g_fillmode );
  // enable lighting
  glEnable( GL_LIGHTING );
  // enable lighting for front
  glLightModeli( GL_FRONT_AND_BACK, GL_TRUE );
  // material have diffuse and ambient lighting 
  glColorMaterial( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE );
  // enable color
  glEnable( GL_COLOR_MATERIAL );
  // normalize (for scaling)
  glEnable( GL_NORMALIZE );
  // line width
  glLineWidth( g_linewidth );
  
  // enable light 0
  glEnable( GL_LIGHT0 );
  
  // setup and enable light 1
  glLightfv( GL_LIGHT1, GL_AMBIENT, g_light1_ambient );
  glLightfv( GL_LIGHT1, GL_DIFFUSE, g_light1_diffuse );
  glLightfv( GL_LIGHT1, GL_SPECULAR, g_light1_specular );
  glEnable( GL_LIGHT1 );
  
  // compute log spacing
  g_log_space = compute_log_spacing( g_fft_size / 2, g_log_factor );
  
  // Load the keyboard textures
  g_key_texture = LoadTextureRAW( "keyboard.raw", 0 );
  g_keyC_texture = LoadTextureRAW( "keyboardC.raw", 0 );
  g_keyCS_texture = LoadTextureRAW( "keyboardCS.raw", 0 );
  g_keyD_texture = LoadTextureRAW( "keyboardD.raw", 0 );
  g_keyDS_texture = LoadTextureRAW( "keyboardDS.raw", 0 );
  g_keyE_texture = LoadTextureRAW( "keyboardE.raw", 0 );
  g_keyF_texture = LoadTextureRAW( "keyboardF.raw", 0 );
  g_keyFS_texture = LoadTextureRAW( "keyboardFS.raw", 0 );
  g_keyG_texture = LoadTextureRAW( "keyboardG.raw", 0 );
  g_keyGS_texture = LoadTextureRAW( "keyboardGS.raw", 0 );
  g_keyA_texture = LoadTextureRAW( "keyboardA.raw", 0 );
  g_keyAS_texture = LoadTextureRAW( "keyboardAS.raw", 0 );
  g_keyB_texture = LoadTextureRAW( "keyboardB.raw", 0 );
  
  // The active texture will be the keyboard with no pressed keys:
  g_texture = g_key_texture;
}

//-----------------------------------------------------------------------------
// Name: map_log_spacing( )
// Desc: ...
//-----------------------------------------------------------------------------
inline double map_log_spacing( double ratio, double power )
{
  // compute location
  return ::pow(ratio, power) * g_fft_size/g_freq_view; 
}

//-----------------------------------------------------------------------------
// Name: compute_log_spacing( )
// Desc: ...
//-----------------------------------------------------------------------------
double compute_log_spacing( int fft_size, double power )
{
  int maxbin = fft_size; // for future in case we want to draw smaller range
  int minbin = 0; // what about adding this one?
  
  for(int i = 0; i < fft_size; i++)
  {
    // compute location
    g_log_positions[i] = map_log_spacing( (double)i/fft_size, power ); 
    // normalize, 1 if maxbin == fft_size
    g_log_positions[i] /= pow((double)maxbin/fft_size, power);
  }
  
  return 1/::log(fft_size);
}

//-----------------------------------------------------------------------------
// Name: pitch_estimation(SAMPLE *x)
// Desc: Pitch estimation of the windowed time domain signal x 
// using autocorrelation
//-----------------------------------------------------------------------------
double pitch_estimation(SAMPLE *x) {
  
  SAMPLE X[g_fft_size];

  // Zero out our buffer
  memset(X, 0, g_fft_size*sizeof(SAMPLE));
  
  // copy our time domain signal to our new vector
  memcpy(X, x, g_fft_size*sizeof(SAMPLE));
  
  // find the autocorrelation signal
  autoCorrelation(X, g_fft_size/2);
  
  // Take the DFT of the autocorrelated signal
  rfft( (SAMPLE *)X, g_fft_size/2, FFT_FORWARD );
  
  // cast to complex
  complex * cbuf = (complex *)X;
  
  SAMPLE max = 0;
  int max_index = 0;
  for (int i=0; i<g_fft_size/2; i++) {
    //printf("x[%d]: %f\n", i, x[i]);
    SAMPLE y = 1.8f * pow( 25 * cmp_abs( cbuf[i] ), .5 );
    if (y > max) {
      //printf("x[%d]: %f\n", i, y);  
      max = y;
      max_index = i;
    }
  }
  
  // Calculate the f0
  double f0 = ((DNA_SRATE/2.0)*max_index) / (g_fft_size/2.0);
  
  return f0;
}

//-----------------------------------------------------------------------------
// Name: autoCorrelation(SAMPLE *x, int bufferSize)
// Desc: Calculates the autocorrelation of the signal x
//-----------------------------------------------------------------------------
void autoCorrelation(SAMPLE *x, int bufferSize)
{
  SAMPLE xcopy[bufferSize];
  
  memcpy(xcopy, x, bufferSize*sizeof(SAMPLE));
  
  float sum;
  int i,j;
  
  for (i=0;i<bufferSize;i++) {
    sum=0;
    for (j=0;j<bufferSize-i;j++) {
      sum+=x[j]*x[j+i];
    }
    xcopy[i]=sum;
  }
  
  // copy the result into our array
  memcpy(x, xcopy, bufferSize*sizeof(SAMPLE));
}

//-----------------------------------------------------------------------------
// Name: void getNote(double pitch)
// Desc: Calculates the note out of a pitch and assigns the correct texture
//-----------------------------------------------------------------------------
void getNote(double pitch) {
  
  // Initialize our base note (A-220Hz)
  double baseA = 220.0;
  
  // Number of octaves
  int n = 4;
  
  // Initialize our semitones coef
  double semitone1 = pow(2, 1/12.);
  double semitone2 = pow(2, 2/12.);
  double semitone3 = pow(2, 3/12.);
  double semitone4 = pow(2, 4/12.);
  double semitone5 = pow(2, 5/12.);
  double semitone6 = pow(2, 6/12.);
  double semitone7 = pow(2, 7/12.);
  double semitone8 = pow(2, 8/12.);
  double semitone9 = pow(2, 9/12.);
  double semitone10 = pow(2, 10/12.);
  double semitone11 = pow(2, 11/12.);
  double semitone12 = pow(2, 12/12.);
  
  // Initialize our inverse semitone coef
  double inv_semitone = (double)(1/semitone1);
  
  // Loop for n octaves
  for (int i = 0; i<n; i++) {
    if (pitch > baseA*inv_semitone && pitch < baseA*semitone1) {
      //printf("A %f, %f, %f\n", pitch, baseA*inv_semitone, baseA*semitone1);
      g_texture = g_keyA_texture;
    }
    
    else if (pitch > baseA*semitone2*inv_semitone && pitch < baseA*semitone2) {
      //printf("A# %f, %f, %f\n", pitch, baseA*semitone2*inv_semitone, baseA*semitone2);
      g_texture = g_keyAS_texture;
    }

    else if (pitch > baseA*semitone3*inv_semitone && pitch < baseA*semitone3) {
      //printf("B %f, %f, %f\n", pitch, baseA*semitone3*inv_semitone, baseA*semitone3);
      g_texture = g_keyB_texture;
    }
    
    else if (pitch > baseA*semitone4*inv_semitone && pitch < baseA*semitone4) {
      //printf("C %f, %f, %f\n", pitch, baseA*semitone4*inv_semitone, baseA*semitone4);
      g_texture = g_keyC_texture;
    }
    
    else if (pitch > baseA*semitone5*inv_semitone && pitch < baseA*semitone5) {
      //printf("C# %f, %f, %f\n", pitch, baseA*semitone5*inv_semitone, baseA*semitone5);
      g_texture = g_keyCS_texture;
    }
    
    else if (pitch > baseA*semitone6*inv_semitone && pitch < baseA*semitone6) {
      //printf("D %f, %f, %f\n", pitch, baseA*semitone6*inv_semitone, baseA*semitone6);
      g_texture = g_keyD_texture;
    }
    
    else if (pitch > baseA*semitone7*inv_semitone && pitch < baseA*semitone7) {
      //printf("D# %f, %f, %f\n", pitch, baseA*semitone7*inv_semitone, baseA*semitone7);
      g_texture = g_keyDS_texture;
    }
    
    else if (pitch > baseA*semitone8*inv_semitone && pitch < baseA*semitone8) {
      //printf("E %f, %f, %f\n", pitch, baseA*semitone8*inv_semitone, baseA*semitone8);
      g_texture = g_keyE_texture;
    }
  
    else if (pitch > baseA*semitone9*inv_semitone && pitch < baseA*semitone9) {
      //printf("F %f, %f, %f\n", pitch, baseA*semitone9*inv_semitone, baseA*semitone9);
      g_texture = g_keyF_texture;
    }
    
    else if (pitch > baseA*semitone10*inv_semitone && pitch < baseA*semitone10) {
      //printf("F# %f, %f, %f\n", pitch, baseA*semitone10*inv_semitone, baseA*semitone10);
      g_texture = g_keyFS_texture;
    }
    
    else if (pitch > baseA*semitone11*inv_semitone && pitch < baseA*semitone11) {
      //printf("G %f, %f, %f\n", pitch, baseA*semitone11*inv_semitone, baseA*semitone11);
      g_texture = g_keyG_texture;
    }
    
    else if (pitch > baseA*semitone12*inv_semitone && pitch < baseA*semitone12) {
      //printf("G# %f, %f, %f\n", pitch, baseA*semitone12*inv_semitone, baseA*semitone12);
      g_texture = g_keyGS_texture;
    }
    
    // Next Octave
    baseA *= 2;
  }
}

//-----------------------------------------------------------------------------
// Name: GLuint LoadTextureRAW( const char * filename, int wrap )
// Desc: load a 400x300 RGB .RAW file as a texture
//-----------------------------------------------------------------------------
GLuint LoadTextureRAW( const char * filename, int wrap )
{
  GLuint texture;
  int width, height;
  BYTE * data;
  FILE * file;
  
  // open texture data
  file = fopen( filename, "rb" );
  if ( file == NULL ) return 0;
  
  // allocate buffer
  width = 400;
  height = 300;
  data = (BYTE*)malloc( width * height * 3 );
  
  // read texture data
  fread( data, width * height * 3, 1, file );
  fclose( file );
  
  // allocate a texture name
  glGenTextures( 1, &texture );
  
  // select our current texture
  glBindTexture( GL_TEXTURE_2D, texture );
  
  // select modulate to mix texture with color for shading
  glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
  
  // when texture area is small, bilinear filter the closest mipmap
  glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_NEAREST );
  // when texture area is large, bilinear filter the first mipmap
  glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
  
  // if wrap is true, the texture wraps over at the edges (repeat)
  //       ... false, the texture ends at the edges (clamp)
  glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                  wrap ? GL_REPEAT : GL_CLAMP );
  glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                  wrap ? GL_REPEAT : GL_CLAMP );
  
  // build our texture mipmaps
  gluBuild2DMipmaps( GL_TEXTURE_2D, 3, width, height,
                    GL_RGB, GL_UNSIGNED_BYTE, data );
  
  // free buffer
  free( data );
  
  return texture;
}

//-----------------------------------------------------------------------------
// Name: void FreeTexture( GLuint texture )
// Desc: Frees the texture of the argument
//-----------------------------------------------------------------------------
void FreeTexture( GLuint texture )
{
  
  glDeleteTextures( 1, &texture );
  
}

//-----------------------------------------------------------------------------
// Name: void rotateView ()
// Desc: Rotates the current view by the angle specified in the globals
//-----------------------------------------------------------------------------
void rotateView () {
  if (g_key_rotate_y) {
    glRotatef ( g_angle_y += g_inc_y, 0.0f, 1.0f, 0.0f );
  }
  else {
    glRotatef (g_angle_y, 0.0f, 1.0f, 0.0f );
  }
  
  if (g_key_rotate_x) {
    glRotatef ( g_angle_x += g_inc_x, 1.0f, 0.0f, 0.0f );
  }
  else {
    glRotatef (g_angle_x, 1.0f, 0.0f, 0.0f );
  }
}

//-----------------------------------------------------------------------------
// Name: void getPitchAndAssignToKeyboard(SAMPLE *buffer)
// Desc: Gets the pitch and then assigns appropriate keyboard texture
//-----------------------------------------------------------------------------
void getPitchAndAssignToKeyboard(SAMPLE *buffer) {
  double f0 = pitch_estimation(buffer);
  
  // Make pitch visually more stable
  if (f0 > (g_prev_pitch - g_pitch_err) && f0 < (g_prev_pitch + g_pitch_err)) {
    g_same_pitch = true;
    g_same_pitch_count++;
  }
  else {
    g_same_pitch = false;
    g_same_pitch_count = 0;
    g_prev_pitch = f0;
  }
  
  if (g_same_pitch_count == 4) {
    getNote(f0);
    g_texture_count = 0;
  }
  
  g_texture_count++;
  
  // If g_texture_reset frames have passed, we will reset the texture
  if (g_texture_count > g_texture_reset) {
    // Assign no key pressed texture, in case there is no pitch to show
    g_texture = g_key_texture;
    g_texture_count = 0;
  }
  
  //printf("f0: %f\n", f0);
}

//-----------------------------------------------------------------------------
// Name: void drawKeyboard ()
// Desc: Draws the keyboard with the correct key pressed
//-----------------------------------------------------------------------------
void drawKeyboard () {
  
  // setup texture mapping
  glEnable( GL_TEXTURE_2D );
  
  // Bind correct key texture
  glBindTexture( GL_TEXTURE_2D, g_texture );
  
  // Draw current texture
  glPushMatrix();
  {
    glColor3f( 1.0f, 1.0f, 1.0f );
    
    glBegin( GL_QUADS );
    glTexCoord2d(0.0,0.0); glVertex2d(-3.0,-3.0);
    glTexCoord2d(1.0,0.0); glVertex2d(+3.0,-3.0);
    glTexCoord2d(1.0,1.0); glVertex2d(+3.0,+3.0);
    glTexCoord2d(0.0,1.0); glVertex2d(-3.0,+3.0);
    glEnd();
  }
  glPopMatrix();
  
  // Disable Textures
  glDisable(GL_TEXTURE_2D);
}

//-----------------------------------------------------------------------------
// Name: void drawWindowedTimeDomain(SAMPLE *buffer)
// Desc: Draws the Windowed Time Domain signal in the top of the screen
//-----------------------------------------------------------------------------
void drawWindowedTimeDomain(SAMPLE *buffer) {
  // Initialize initial x
  GLfloat x = -5;
  
  // Calculate increment x
  GLfloat xinc = fabs((2*x)/g_buffer_size);
  
  glPushMatrix();
  {
    glTranslatef(0.0, 2.5f, 0.0f);
    glColor3f(0.2, 1.0, 0.0);
    
    glBegin(GL_LINE_STRIP);
    
    // Draw Windowed Time Domain
    for (int i=0; i<g_buffer_size; i++)
    {
      glVertex3f(x, 2*buffer[i], 0.0f);
      x += xinc;
    }
    
    glEnd();
    
  }
  glPopMatrix();
}

//-----------------------------------------------------------------------------
// Name: void drawRotatingTimeDomain(SAMPLE *buffer) 
// Desc: Draws the Rotating Time Domain signal in the back of the screen
//-----------------------------------------------------------------------------
void drawRotatingTimeDomain(SAMPLE *buffer) {

  // Initialize initial x length and x
  GLfloat len = -50;
  GLfloat x = len;
  
  // Number of Time Domain Signals
  GLint nLines = 100;
  
  // Color interpolation aux float var
  GLfloat fval;
  
  // Calculate increment x
  GLfloat xinc = fabs((2*x)/g_buffer_size);
  
  glPushMatrix();
  {
    // Translate it to the background
    glTranslatef(0.0, 0.0f, -60.0f);
    
    // Assign color
    glColor3f(0.2, 0.2, 0.0);
    
    // Change line width
    glLineWidth(3.5f);
    
    // Loop for every time domain signal
    for (int j = 0; j< nLines; j++) {
    
      // Rotate
      glRotatef(j*rand(), 0.0f, 0.0f, 1.0f);
      
      // Reset x
      x = len;
    
      // Draw Current Windowed Time Domain Signal
      glBegin(GL_LINE_STRIP);
      for (int i=0; i<g_buffer_size; i++)
      {
        // Calculate Colors
        if (i< g_buffer_size/2) {
          fval = i / (float)(g_buffer_size);
          glColor3f ( 0.01*fval, 0.1*fval, 0.2*fval);
        }
        else {
          fval = i / (float)(g_buffer_size);
          glColor3f( 0.01*fval, 0.01*fval, 0.1*fval);

        }
        glVertex3f(x, 4*buffer[i], 0.0f);
        x += xinc;
      }
      
      glEnd();
    }
    
  }
  glPopMatrix();
  
  // back to default line width
  glLineWidth(1.0f);

}

//-----------------------------------------------------------------------------
// Name: void drawScrollingTimeDomain(SAMPLE *buffer)
// Desc: Draws the Scrolling Time Domain signal in the center of the screen
//-----------------------------------------------------------------------------
void drawScrollingTimeDomain(SAMPLE *buffer) {
  //increment g_scroll_writer
  g_scroll_writer += g_buffer_size;
  g_scroll_writer %= g_scroll_buffer_size;
  
  // Initialize initial x
  GLfloat x = -5;
  
  // Calculate increment x
  GLfloat xinc = fabs((2*x)/g_scroll_buffer_size);
  
  
  // Scroll Time Domain signal
  glPushMatrix();
  {
    glBegin(GL_LINE_STRIP);
    
    glTranslatef(0.0, 0.0f, 0.0f);
    glColor3f(0.2, 0.2, 1.0);
    
    for (int i = 0; i < g_scroll_buffer_size; i++) {
      glVertex2f (x, 2*g_scroll_buffer[g_scroll_reader]);
      x += xinc;
      // Increment the reader buffer_size
      g_scroll_reader++;
      g_scroll_reader %= g_scroll_buffer_size;
    }
    
    // Increment the scroll reader index
    g_scroll_reader += g_buffer_size;
    g_scroll_reader %= g_scroll_buffer_size;
    
    glEnd();
  }
  glPopMatrix();
  
}

//-----------------------------------------------------------------------------
// Name: void drawSpectrumWt(complex *cbuf)
// Desc: Draws the Spectrum Waterfall in the bottom of the screen
//-----------------------------------------------------------------------------
void drawSpectrumWt(complex *cbuf) {
  //init vars
  GLfloat x = -5.0f;
  GLint y = -1.0f;
  GLfloat fval, xinc;
  
  // Calculate increment x
  xinc = fabs((2*x)/(double)(g_fft_size/2));
  
  // copy current magnitude spectrum into waterfall memory
  for( int i = 0; i < g_fft_size/2; i++ )
  {
    // copy x coordinate
    g_spectrums[g_wf][i].x = x;
    // copy y
    g_spectrums[g_wf][i].y = g_gain * g_freq_scale * 1.8f *
    ::pow( 25 * cmp_abs( cbuf[i] ), .5 ) + y;
    //printf("spectrum: %f y: %f\n", g_spectrums[g_wf][i].y, y);
    // increment x
    x += xinc;
  }
  
  x = -5.0f;
  // Calculate increment x
  xinc = fabs((2*x)/(double)(g_fft_size/2));
  
  glPushMatrix();
  {
    
    glTranslatef(0.0, -3.0f, 0.0f);
    glColor3f(1., .2, 0.0);
    
    // loop through each layer of waterfall
    for( int i = 0; i < g_depth; i++ )
    {
      // get the magnitude spectrum of layer
      Pt2D * pt = g_spectrums[(g_wf+i)%g_depth];
      // future
      if( i < g_wf_delay )
      {
        // brightness based on depth
        fval = (g_depth - i + g_wf_delay) / (float)(g_depth);
        glColor3f( 1.0 * fval, .2 * fval, 0.0f * fval ); // depth cue
      }
      // present
      else if( i == g_wf_delay )
      {
        // draw the now line?
        if( false )
        {
          glLineWidth( 2.0f );
          glColor3f( 1.0f, 0.2f, 0.0f );
        }
      }
      // past
      else
      {
        // brightness based on depth
        fval = (g_depth - i + g_wf_delay) / (float)(g_depth);
        glColor3f( 1.0f * fval, .2f * fval, 0.0f * fval ); //depth cue
      }
      
      // render the actual spectrum layer
      glBegin( GL_LINE_STRIP );
      for( GLint j = 0; j < g_fft_size/2; j++, pt++ )
      {
        // draw the vertex
        float d = (float) i;
        glVertex3f( pt->x, pt->y, -d*0.3);
      }
      glEnd();
      
      // back to default line width
      glLineWidth(1.0f);
    }
    
    // advance index
    g_wf--;
    // mod
    g_wf = (g_wf + g_depth) % g_depth; 
    
  }
  glPopMatrix();
}

//-----------------------------------------------------------------------------
// Name: displayFunc( )
// Desc: callback function invoked to draw the client area
//-----------------------------------------------------------------------------
void displayFunc( )
{
  // local state
  static GLfloat zrot = 0.0f, c = 0.0f;
  
  GLfloat x, xinc;
  
  // local variables
  SAMPLE buffer[g_fft_size];
  
  // wait for data
  while( !g_ready ) usleep( 1000 );
  
  // lock
  g_mutex.lock();
  
  // get the latest (possibly preview) window
  memset( buffer, 0, g_fft_size * sizeof(SAMPLE) );
  
  // copy currently playing audio into buffer
  memcpy( buffer, g_buffer, g_buffer_size * sizeof(SAMPLE) );
  
  // Hand off to audio callback thread
  g_ready = FALSE;
  
  // unlock
  g_mutex.unlock();
  
  // copy buffer to the last part of the g_scroll_buffer
  for (int i = g_scroll_writer, j = 0; i<g_scroll_writer+g_buffer_size; i++, j++) {
    g_scroll_buffer[i] = buffer[j];
  }
  
  // clear the color and depth buffers
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
  
  // Draw the rotating background time-domain,
  // Before doing the other rotations and applying the window
  drawRotatingTimeDomain(buffer);
  
  // apply the transform window
  apply_window( (float*)buffer, g_window, g_buffer_size );
  
  // Draw keyboard if needed
  if (g_keyboard_on) {
    
    // Draw the Keyboard
    drawKeyboard();
    
    // Get the Pitch and assign to keyboard key
    getPitchAndAssignToKeyboard(buffer);
  }
  
  // No keyboard to draw, draw everything else
  else {
    
    // save state
    glPushMatrix();
    {
      //apply the rotations:
      rotateView();
      
      // Windowed Time Domain
      drawWindowedTimeDomain(buffer);
      
      // Scrolling
      drawScrollingTimeDomain(buffer);
      
      // take forward FFT; result in buffer as FFT_SIZE/2 complex values
      rfft( (float *)buffer, g_fft_size/2, FFT_FORWARD );
      
      // cast to complex
      complex * cbuf = (complex *)buffer;
      
      // Draw Spectrum Waterfall
      drawSpectrumWt(cbuf);
    }
    // pop matrix for the rotations
    glPopMatrix();
  }
  
  // flush gl commands
  glFlush( );
  // swap the buffers
  glutSwapBuffers( );
    
}
