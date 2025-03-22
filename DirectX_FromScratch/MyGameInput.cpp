#pragma once
#include "d3dUtil.h"
#include "MyGameInput.h"
using namespace GameInput::v1;

MyGameInput::MyGameInput() {}

bool MyGameInput::Init() {
	HRESULT result = InitializeInput();
	if (FAILED(result)) return false;
	else return true;
}

MyGameInput::~MyGameInput() {
	ShutdownInput();
}

HRESULT MyGameInput::InitializeInput()
{
	return GameInputCreate(&mGameInput);
}

void MyGameInput::ShutdownInput()
{
	if (mGamepad) mGamepad->Release();
	if (mGameInput) mGameInput->Release();
}

void MyGameInput::PollGamepadInput()
{
	// Ask for the latest reading from devices that provide fixed-format
	// gamepad state. If a device has been assigned to g_gamepad, filter
	// readings to just the ones coming from that device. Otherwise, if
	// g_gamepad is null, it will allow readings from any device.
	IGameInputReading* reading;
	if (SUCCEEDED(mGameInput->GetCurrentReading(GameInputKindGamepad, mGamepad, &reading)))
	{
		GameInputGamepadState gamepadState;
		reading->GetGamepadState(&gamepadState);
		float _test = gamepadState.leftTrigger;

		d3dUtil::convertIntToDisplayStr((int)_test);

		// If no device has been assigned to g_gamepad yet, set it
		// to the first device we receive input from. (This must be
		// the one the player is using because it's generating input.)
		if (!mGamepad) reading->GetDevice(&mGamepad);

		// Retrieve the fixed-format gamepad state from the reading.
		GameInputGamepadState state;
		reading->GetGamepadState(&state);
		reading->Release();

		// Application-specific code to process the gamepad state goes here.
	}

	// If an error is returned from GetCurrentReading(), it means the
	// gamepad we were reading from has disconnected. Reset the
	// device pointer, and go back to looking for an active gamepad.
	else if (mGamepad)
	{
		mGamepad->Release();
		mGamepad = nullptr;
	}
}