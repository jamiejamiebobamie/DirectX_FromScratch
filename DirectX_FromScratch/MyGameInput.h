#pragma once
#include <GameInput.h>
using namespace GameInput::v1;

class MyGameInput
{
public:
	MyGameInput();
	MyGameInput(const MyGameInput& rhs) = delete;
	MyGameInput& operator=(const MyGameInput& rhs) = delete;
	~MyGameInput();
	void PollGamepadInput();
	bool Init();

private:
	HRESULT InitializeInput();
	void ShutdownInput();

private:
	IGameInput* mGameInput = nullptr;
	IGameInputDevice* mGamepad = nullptr;
};

