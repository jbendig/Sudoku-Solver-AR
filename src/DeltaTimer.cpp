// Copyright 2017 James Bendig. See the COPYRIGHT file at the top-level
// directory of this distribution.
//
// Licensed under:
//   the MIT license
//     <LICENSE-MIT or https://opensource.org/licenses/MIT>
//   or the Apache License, Version 2.0
//     <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>,
// at your option. This file may not be copied, modified, or distributed
// except according to those terms.

#include "DeltaTimer.h"
#include <GLFW/glfw3.h>

DeltaTimer::DeltaTimer()
	: deltaTime(0.0),
	  lastTime(0.0)
{
	Update();
}

void DeltaTimer::Update()
{
	const double currentTime = glfwGetTime();
	if(lastTime != 0.0)
		deltaTime = currentTime - lastTime;
	lastTime = currentTime;
}

double DeltaTimer::Delta() const
{
	return deltaTime;
}

