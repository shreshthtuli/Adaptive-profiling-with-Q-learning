#ifndef TUNER_H
#define TUNER_H

#include "PortableApi.h"
#include "pin.H"

enum class Phase
{
	InvalidPhase,
	XPhase,
	YPhase,
	ZPhase
};

enum class TunerAction
{
	None,
	CreateOrResume,
	Suspend
};

class Tuner
{
public:
	LOCALFUN VOID Init(BOOL enable);
	LOCALFUN TunerAction OnTimerInterrupt(TIME_STAMP *totalProcessingThreadRunningWaitingTime,
		TIME_STAMP *totalAppThreadRunningWaitingTime,
		BOOL anyProcThreads
		,
		TIME_STAMP& ptitDebug,
		TIME_STAMP& atwtDebug
	);
	LOCALFUN Phase GetPhase();

	LOCALVAR const UINT32 InitialThreadCount = 2;
	LOCALVAR const UINT32 MaxThreadCount = 32;
private:
	Tuner();

	LOCALVAR BOOL s_enabled;
	LOCALVAR Phase s_currentPhase;
	LOCALVAR Phase s_previousPhase;
	LOCALVAR TIME_STAMP s_totalAppThreadLastWaitingTime; // Holds the last wait time.
	LOCALVAR TIME_STAMP s_totalProcessingThreadLastWaitingTime; // Holds the last wait time.
	LOCALVAR float s_tp;
	LOCALVAR float s_tay1;
	LOCALVAR float s_tay2;
	LOCALVAR float s_taz;
	LOCALVAR UINT32 s_stuckInX;
	LOCALVAR UINT32 s_stuckInZ;
};

#endif TUNER_H
