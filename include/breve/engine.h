/*****************************************************************************
 *                                                                           *
 * The breve Simulation Environment                                          *
 * Copyright (C) 2000, 2001, 2002, 2003 Jonathan Klein                       *
 *                                                                           *
 * This program is free software; you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation; either version 2 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program; if not, write to the Free Software               *
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA *
 *****************************************************************************/

#ifndef _ENGINE_H
#define _ENGINE_H

#include <pthread.h>
#include <sys/stat.h>
#include <gsl/gsl_rng.h>
#include <vector>
#include <map>
#include <string>
#include <algorithm>

#include "timeval.h"

class slCamera;
class slWorld;
class slRenderGL;
class brEvent;
class brURLFetcher;
class brSoundMixer;
class brNamespace;

struct brInstance;
struct brInternalFunction;
struct brDlPlugin;
struct brInstance;
struct brObject;

// the maximum error size 

#define BR_ERROR_TEXT_SIZE 4096

/**
 * \brief A structure to hold information about errors during parsing/execution.
 */

struct brErrorInfo {
	brErrorInfo() { 
		file = NULL;
		clear();
	}

	~brErrorInfo();

	void clear();

	char *file;
	int line;
	unsigned char type;
	char message[ BR_ERROR_TEXT_SIZE ];
};

/**
 * \brief Parse error and evaluation codes used by \ref brEvalError.	
 */

enum parseErrorMessageCodes {
	// parse errors 

	PE_UNKNOWN = 0,
	PE_PARSE,
	PE_PYTHON,
	PE_SYNTAX,
	PE_INTERNAL,
	PE_UNKNOWN_SYMBOL,
	PE_UNKNOWN_FUNCTION,
	PE_UNKNOWN_OBJECT,
	PE_REDEFINITION,
	PE_TYPE,
	PE_PROTOTYPE,
	PE_NO_CONTROLLER,
	PE_FILE_VERSION,

	// evaluation errors 

	EE_UNKNOWN_CONTROLLER,
	EE_USER,
	EE_INTERNAL,
	EE_TYPE,
	EE_ARRAY,
	EE_CONVERT,
	EE_FREED_INSTANCE,	
	EE_NULL_INSTANCE,
	EE_MATH,
	EE_SIMULATION,
	EE_BOUNDS,
	EE_UNKNOWN_OBJECT,
	EE_UNKNOWN_METHOD,
	EE_UNKNOWN_KEYWORD,
	EE_MISSING_KEYWORD,
	EE_FILE_NOT_FOUND
};

/*!
	\brief Error codes describing where a parse error occurred.
*/

enum parseErrorCodes {
	BPE_OK = 0,
	BPE_SIM_ERROR,
	BPE_LIB_ERROR
};

/**
 * \brief A single menu item.
 */

struct brMenuEntry {
	brInstance *instance;
	char *method;
	char *title;
	bool enabled;
	bool checked;
};

/*!
	\brief The main breve engine structure.
*/

#ifdef __cplusplus
typedef struct brObjectType brObjectType;

class brEngine {
	public:
		brEngine( int inArgc = 0, char *inArgv[] = NULL);
		~brEngine();

		std::vector< brObjectType* > 		_objectTypes;
		std::vector< brInstance* > 		_freedInstances;

		int 					iterate();

		void					draw();
		void					clear();
		void					lock();
		void					unlock();

		brInstance*				getController() const { return _controller; }
		int						setController( brInstance *inController );

		slCamera*				getCamera() const { return camera; }

		const char 				*runSaveDialog();
		const char 				*runLoadDialog();
		void					setMouseLocation( int inX, int inY );

		double 					runningTime();

		void 					pauseTimer();
		void 					unpauseTimer();

		void addSearchPath( const char *inPath );

		brURLFetcher*			_url;
		brSoundMixer*			_soundMixer;

		slWorld* 				world;
		slCamera*				camera;

		slRenderGL*				_renderer;

		gsl_rng*				RNG;

		bool 					_simulationWillStop;

		int 					_useMouse;
		int 					_mouseX;
		int 					_mouseY;

		double 					_iterationStepSize;

		FILE*					_logFile;

		brInstance*				_controller;

		std::map< std::string, brObject* > 	objectAliases;
		std::map< std::string, brObject* > 	objects;
		brNamespace*				internalMethods;

		std::vector< brInstance* > 		postIterationInstances;
		std::vector< brInstance* > 		iterationInstances;
		std::vector< brInstance* > 		instances;
	
		std::vector< brInstance* > 		instancesToAdd;
		std::vector< brInstance* > 		instancesToRemove;

		std::vector< brEvent* > 		events;

public:
		// runtime error info 

		brErrorInfo 				error;

		std::string 				_outputPath;
		std::string 				_launchDirectory;

		std::vector< brDlPlugin* > 		dlPlugins;

		// the drawEveryFrame flag is a hint to the display engine
		// if set, the application attempts to draw a frame with every
		// iteration of the breve engine. 

		unsigned char 				drawEveryFrame;

		struct timeval 				unpauseTime;
		struct timeval 				accumulatedTime;

		int 					_argc;
		char**					_argv;

		int 					nThreads;
		pthread_mutex_t 			_engineLock;
		int 					lastScheduled;

		std::vector<void*> 			windows;
		std::vector< std::string > 		_searchPaths;

		// which keys are pressed?

		unsigned char 				keys[ 256 ];

		//
		// Callback functions to be set by the application frontend
		//

		// callback to update a menu for an instance

		void (*updateMenu)(brInstance *);

		// callback to run save and load dialogs 

		const char *(*getSavename)(void);
		const char *(*getLoadname)(void);

		// callback to show a generic dialog

		int (*dialogCallback)(char *, char *, char *, char *);
	
		// callback to play a beep sound
	
		int (*soundCallback)(void);
	
		// returns the string identifying the implementation
	
		const char *(*interfaceTypeCallback)(void);
	
		// callback to setup and use the OS X interface features
	
		int (*interfaceSetStringCallback)(char *, int);
		void (*interfaceSetCallback)(char *);

		int (*pauseCallback)(void);
		int (*unpauseCallback)(void);

		void *(*newWindowCallback)(char *, void *);
		void (*freeWindowCallback)(void *);
		void (*renderWindowCallback)(void *);
};

/*!
	\brief A scheduled event in the breve engine
*/

class brEvent {
	public:
		brEvent(const char *name, double time, double interval, brInstance *i);
		~brEvent() {};

		std::string 	_name;
		double 		_time;
		double 		_interval;
		brInstance*	_instance;
};
#endif

enum versionRequiermentOperators {
	VR_LT = 1,
	VR_GT,
	VR_LE,
	VR_GE,
	VR_EQ,
	VR_NE
};

brEvent *brEngineAddEvent(brEngine *, brInstance *, const char *, double, double);
int brEngineRemoveEvent(brEngine *, brInstance *, const char *, double);
void brEventFree(brEvent *);

void brEngineSetIOPath(brEngine *, const char *);
char *brOutputPath(brEngine *, const char *);

const std::vector< std::string > &brEngineGetSearchPaths( brEngine * );

std::vector< std::string > brEngineGetAllObjectNames( brEngine *e );

void brAddToInstanceLists(brInstance *);
void brRemoveFromInstanceLists(brInstance *);

int brFileLogWrite(void *, const char *, int);

char *brFindFile(brEngine *, const char *, struct stat *);
void brFreeSearchPath(brEngine *);

brMenuEntry *brAddMenuItem( brInstance *, const char *, const char * );
brMenuEntry *brAddContextualMenuItem(brObject *, char *, char *);

void stSetParseEngine(brEngine *);

void brPrintVersion(void);

void brFreeObjectSpace(brNamespace *);

void brEngineRenderWorld(brEngine *, int inCrosshair = 0 );

brInternalFunction *brEngineInternalFunctionLookup(brEngine *, char *);

void brEvalError(brEngine *, int, const char *, ...);

void brClearError(brEngine *);
int brGetError(brEngine *);

int brEngineSetInterface(brEngine *, char *);

brErrorInfo *brEngineGetErrorInfo(brEngine *);

const char *brEngineGetPath( brEngine* );

brNamespace *brEngineGetInternalMethods(brEngine *);

int brEngineGetDrawEveryFrame(brEngine *);

void brEngineSetSoundCallback(brEngine *, int (*)(void));
void brEngineSetDialogCallback(brEngine *, int (*)(char *, char *, char *, char *));

void brEngineSetPauseCallback(brEngine *, int (*)(void));
void brEngineSetUnpauseCallback(brEngine *, int (*)(void));
void brEngineSetGetLoadnameCallback(brEngine *, char *(*)(void));
void brEngineSetGetSavenameCallback(brEngine *, char *(*)(void));

void brEngineSetInterfaceInterfaceTypeCallback(brEngine *, char *(*)(void));
void brEngineSetInterfaceSetStringCallback(brEngine *, int (*)(char *, int));
void brEngineSetInterfaceSetNibCallback(brEngine *, void (*)(char *));
void brEngineSetUpdateMenuCallback(brEngine *, void (*)(brInstance *));

slCamera *brEngineGetCamera(brEngine *);
slWorld *brEngineGetWorld(brEngine *);

#endif
