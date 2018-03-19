#include "Error.h"

extern UINT32 g_processNumber;
extern BOOL g_isGenesis;
extern const CHAR *g_mainExe;
extern std::string g_outputDir;
static std::string g_errorFilePath;

extern void PathCombine(std::string& output, const std::string& path1, const std::string& path2);

ALLERIA_ERROR lastError = ALLERIA_ERROR_SUCCESS;
BOOL g_isFaulty = FALSE;

const char *ErrorStrings[] = {
	"ALLERIA ERROR: It looks like Alleria has a bug!",
	"ALLERIA ERROR: Invalid configuration file.",
	"ALLERIA ERROR: The version of the configuration file is not supported.",
	"ALLERIA ERROR: The Reopenable attribute can only be used with a valid On event.",
	"ALLERIA ERROR: The Profile section does not exist.",
	"ALLERIA ERROR: Invalid Event.",
	"ALLERIA ERROR: The Process section does not exist.",
	"ALLERIA ERROR: The Thread section does not exist.",
	"ALLERIA ERROR: The Image section does not exist.",
	"ALLERIA ERROR: The Function section does not exist.",
	"ALLERIA ERROR: Invalid Profile section.",
	"ALLERIA ERROR: Cannot create profile.",
	"ALLERIA ERROR: Initialization failed.",
	"ALLERIA ERROR: Out of memory.",
	"ALLERIA ERROR: Processing thread failed to terminate successfully.",
	"ALLERIA ERROR: The specified MemRef buffer size is invalid. For help, use -h or -help.",
	"ALLERIA ERROR: The specified InsRecord buffer size is invalid. For help, use -h or -help.",
	"ALLERIA ERROR: The number of buffers per app thread must be a positive integer. For help, use -h or -help.",
	"ALLERIA ERROR: The specified assembly syntax style is invalid. For help, use -h or -help.",
	"ALLERIA ERROR: The specified output format is invalid. For help, use -h or -help.",
	"ALLERIA ERROR: The specified timer resolution is invalid. For help, use -h or -help.",
	"ALLERIA ERROR: Invalid mode of virtual address translation. For help, use -h or -help.",
	"ALLERIA ERROR: Elevation required for address translation. For help, use -h or -help.",
	"ALLERIA ERROR: Failed to load driver. For help, use -h or -help.",
	"ALLERIA ERROR: The specified ODS mode is invalid. For help, use -h or -help.",
	"ALLERIA ERROR: The specified buffer size is too small.",
	"ALLERIA ERROR: The timer must be enabled to enable adaptive profiling.",
	"ALLERIA ERROR: Textual profiles cannot be used with adaptive profiling.",
};

void AlleriaSetLastError(ALLERIA_ERROR e)
{
	lastError = e;
}

ALLERIA_ERROR AlleriaGetLastError()
{
	return lastError;
}

const char *AlleriaGetError(ALLERIA_ERROR e)
{
	ASSERTX(e != ALLERIA_ERROR_SUCCESS);
	int errorCode = static_cast<int>(e);
	if (errorCode < 0 || errorCode >= sizeof(ErrorStrings) / sizeof(char*))
		return "ALLERIA ERROR: Unknown error.";
	else
		return ErrorStrings[errorCode];
}

BOOL IsFirstAccessToLogFile(std::ofstream& stream)
{
	static BOOL firstTime = TRUE;
	if (g_isGenesis && firstTime)
	{
		PathCombine(g_errorFilePath, g_outputDir, ALLERIA_LOG_FILE_NAME);
		stream.open(g_errorFilePath.c_str(), std::ofstream::out | std::ofstream::trunc);
		const time_t now = time(0);
		const char *dt = ctime(&now);
		stream << "Alleria Profiling Session Log " << dt << NEW_LINE;
		stream << "pn, pid, exe, h24:m:s: message." << NEW_LINE;
		firstTime = FALSE;
		return TRUE;
	}
	return FALSE;
}

void WriteLogMessageHeader(std::ofstream& stream)
{
	const time_t now = time(0);
	const tm *ltm = localtime(&now);
	stream << g_processNumber << ", " << GetProcessIdPortable(GetCurrentProcessPortable()) <<
		", " << g_mainExe << ", " << ltm->tm_hour << ":" << ltm->tm_min << ":" << ltm->tm_sec << ": ";
}

void ALLERIA_WriteError(ALLERIA_ERROR e)
{
	const char * msg = AlleriaGetError(e);
	PMUTEX mutex = AcquireAccessToLogFile();
	std::ofstream stream;
	if (!IsFirstAccessToLogFile(stream))
	{
		stream.open(g_errorFilePath.c_str(), std::ofstream::out | std::ofstream::app);
	}
	if (stream.good())
	{
		AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
		WriteLogMessageHeader(stream);
		stream << msg << NEW_LINE;
		stream.flush();
	}
	
	// Even though the application is being terminated, release the mutex for other processes in the tree.
	ReleaseAccesstoLogFile(mutex);
	
	// Pin's PIN_ExitApplication doesn't work properly before the app has started.
	// Also std::exit and std::abort are hijacked by Pin to execute the Fini function, which is not desirable.
	g_isFaulty = TRUE; // Used in Fini.
	std::exit(e);
}

PMUTEX AcquireAccessToLogFile()
{
	PMUTEX mutex = Mutex_CreateNamed(ALLERIA_LOG_FILE_MUTEX_NAME);
	Mutex_Acquire(mutex);
	return mutex;
}

void ReleaseAccesstoLogFile(PMUTEX mutex)
{
	Mutex_Release(mutex);
	Mutex_Close(mutex);
}

const char *ReportStrings[] = {
	"Finalizing.",
	"Pintool exiting.",
	"Waiting for child.",
	"Child joined.",
	"Child abandoned.",
	"Runtime heap lost track of some allocations.",
	"XSAVE accessing an unkown state component.",
	"On event triggered.",
	"Till event triggered.",
	"Stats.", // ALLERIA_REPORT_STATS
	"Initializing.",
	"App starting.",
	"Failed to convert some virtual addresses.",
	"An internal exception occurred.", // ALLERIA_REPORT_INTERNAL_EXCEPTION
	"Context Chnaged.", // ALLERIA_REPORT_CONTEXT_CHANGED
	"Adaptive Profiling: Thread and buffer created.",
	"Adaptive Profiling: Phase changed.",
	"Adaptive Profiling: Thread suspended.",
	"Adaptive Profiling: Thread resumed."
};

const char *AlleriaGetReportMessage(ALLERIA_REPORT_TYPE r)
{
	int errorCode = static_cast<int>(r);
	if (errorCode < 0 || errorCode >= sizeof(ReportStrings) / sizeof(char*))
		return "ALLERIA MESSAGE: Unknown message.";
	else
		return ReportStrings[errorCode];
}

void ALLERIA_WriteMessageEx(ALLERIA_REPORT_TYPE r, const char* msg)
{
	PMUTEX mutex = AcquireAccessToLogFile();
	std::ofstream stream;
	if (!IsFirstAccessToLogFile(stream))
	{
		stream.open(g_errorFilePath.c_str(), std::ofstream::out | std::ofstream::app);
	}
	if (!stream.good())
	{
		AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
		return;
	}
	WriteLogMessageHeader(stream);
	stream << msg << NEW_LINE;
	stream.flush();
	ReleaseAccesstoLogFile(mutex);
}

void ALLERIA_WriteMessage(ALLERIA_REPORT_TYPE r)
{
	ALLERIA_WriteMessageEx(r, AlleriaGetReportMessage(r));
}