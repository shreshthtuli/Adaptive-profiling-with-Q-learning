#include "Tuner.h"
#pragma DISABLE_COMPILER_WARNING_DATA_LOSS
#include <atomic/ops.hpp>
#pragma RESTORE_COMPILER_WARNING_DATA_LOSS

BOOL Tuner::s_enabled;
Phase Tuner::s_currentPhase;
Phase Tuner::s_previousPhase;
TIME_STAMP Tuner::s_totalAppThreadLastWaitingTime;
TIME_STAMP Tuner::s_totalProcessingThreadLastWaitingTime;
float Tuner::s_tp;
float Tuner::s_tay1;
float Tuner::s_tay2;
float Tuner::s_taz;
UINT32 Tuner::s_stuckInX;
UINT32 Tuner::s_stuckInZ;

VOID Tuner::Init(BOOL enable)
{
	s_enabled = enable;

	if (s_enabled)
	{
		s_currentPhase = Phase::YPhase;
		s_previousPhase = Phase::InvalidPhase;
		s_totalAppThreadLastWaitingTime = 0;
		s_totalProcessingThreadLastWaitingTime = 0;
		s_tp = 1.0f;
		s_tay1 = 0.1f;
		s_tay2 = 0.1f;
		s_taz = 0.01f;
		s_stuckInX = 0;
		s_stuckInZ = 0;
	}
	else
	{
		s_currentPhase = Phase::InvalidPhase;
	}
}

TunerAction Tuner::OnTimerInterrupt(TIME_STAMP *totalProcessingThreadRunningWaitingTime,
	TIME_STAMP *totalAppThreadRunningWaitingTime,
	BOOL anyProcThreads
	,
	TIME_STAMP& ptitDebug,
	TIME_STAMP& atwtDebug
)
{
	ASSERTX(s_enabled);
	ASSERTX(s_currentPhase != Phase::InvalidPhase);

	TIME_STAMP ptit = ATOMIC::OPS::Load(totalProcessingThreadRunningWaitingTime);
	TIME_STAMP atwt = ATOMIC::OPS::Load(totalAppThreadRunningWaitingTime);
	TunerAction action = TunerAction::None;

	ptitDebug = ptit;
	atwtDebug = atwt;

	if (s_currentPhase == Phase::XPhase)
	{
		++s_stuckInX;
		if (((ptit > (s_totalProcessingThreadLastWaitingTime +
			(s_totalProcessingThreadLastWaitingTime * s_tp))) &&
			(s_previousPhase != Phase::YPhase)) || s_stuckInX > 10)
		{
			s_previousPhase = s_currentPhase;
			s_currentPhase = Phase::ZPhase;
			//if (atwt > s_totalAppThreadLastWaitingTime)
			s_totalAppThreadLastWaitingTime = atwt;
			s_stuckInX = 0;
		}
		else
		{
			if (s_previousPhase != s_currentPhase && ptit != 0)
			{
				s_totalProcessingThreadLastWaitingTime = ptit;
				s_previousPhase = s_currentPhase;
			}
			else if (ptit < s_totalProcessingThreadLastWaitingTime)
			{
				s_totalProcessingThreadLastWaitingTime = ptit;
			}

			action = TunerAction::CreateOrResume;
		}
	}
	else if (s_currentPhase == Phase::YPhase)
	{
		s_previousPhase = s_currentPhase;
		if ((atwt > (s_totalAppThreadLastWaitingTime + (s_totalAppThreadLastWaitingTime * s_tay1))) ||
			(atwt < (s_totalAppThreadLastWaitingTime + (s_totalAppThreadLastWaitingTime * s_tay2))))
		{
			s_currentPhase = Phase::XPhase;
			// ATOMIC::OPS::Store(totalProcessingThreadRunningWaitingTime, (TIME_STAMP)0);
		}
		else
		{
			// Do nothing.
		}
	}
	else
	{
		++s_stuckInZ;
		if (((atwt > (s_totalAppThreadLastWaitingTime + (s_totalAppThreadLastWaitingTime * s_taz))) &&
			(s_previousPhase != Phase::XPhase)) || s_stuckInZ > 10)
		{
			s_previousPhase = s_currentPhase;
			s_currentPhase = Phase::YPhase;
			s_totalAppThreadLastWaitingTime = atwt;
			s_stuckInZ = 0;
		}
		else if (anyProcThreads)
		{
			s_previousPhase = s_currentPhase;
			action = TunerAction::Suspend;
		}
		else
		{
			s_previousPhase = s_currentPhase;
			// Do nothing.
		}
	}

	ATOMIC::OPS::Store(totalProcessingThreadRunningWaitingTime, (TIME_STAMP)0);
	ATOMIC::OPS::Store(totalAppThreadRunningWaitingTime, (TIME_STAMP)0);
	return action;
}

Phase Tuner::GetPhase()
{
	return s_currentPhase;
}
