#include <windows.h>
#include "GameTimer.h"

GameTimer::GameTimer() // intialize GameTimer
	: mSecondsPerCount(0.0), mDeltaTime(-1.0), mBaseTime(0),
		mPausedTime(0), mPrevTime(0), mCurrTime(0), mStopped(false)
{
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	mSecondsPerCount = 1.0 / (double)countsPerSec;
}

float GameTimer::TotalTime()const // compute total time
{
	if (mStopped) { // if stopped, compute total time from when the player stopped
		return (float)(((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
	}
	else { // if not stopped, compute total time from current time
		return (float)(((mCurrTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
	}

	// mStopTime and mCurrTime includes mPausedTime, so subtract that out
}

float GameTimer::DeltaTime()const
{
	return (float)mDeltaTime;
}

void GameTimer::Reset() // reset GameTimer
{
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped = false;
}

void GameTimer::Start() // toggle mStopped = false and accumulate stopped time as mPausedTime
{
	if (mStopped)
	{
		__int64 startTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

		mPausedTime += (startTime - mStopTime);

		mPrevTime = startTime;
		mStopTime = 0;
		mStopped = false;
	}
}

void GameTimer::Stop() // toggle mStopped = true and set mStopTime
{
	if (!mStopped)
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		mStopTime = currTime;
		mStopped = true;
	}
}

void GameTimer::Tick() // update mCurrTime and compute mDeltaTime
{
	if (mStopped)
	{
		mDeltaTime = 0.0;
		return;
	}

	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;

	mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;

	mPrevTime = mCurrTime;

	if (mDeltaTime < 0.0)
	{
		mDeltaTime = 0.0;
	}
}