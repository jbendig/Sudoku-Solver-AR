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

#include "Geometry.h"
#include <algorithm>
#include <cassert>
#include <cmath>

float MeanTheta(const std::vector<Line>& lines)
{
	assert(!lines.empty());

	float sumTheta = 0.0f;
	float minTheta = M_PI / 2.0f;
	float maxTheta = 0.0f;
	float shiftTheta = 0.0f;
	for(const Line& line: lines)
	{
		const float theta = line.theta;
		sumTheta += theta;
		minTheta = std::min(minTheta,theta);
		maxTheta = std::max(maxTheta,theta);
	}

	//If mean is in the wrapping portion, calculate it by wrapping all of the angles back into
	//the 0..2PI range, calculating the mean, shifting back, and once again wrapping back to
	//the 0..2PI range.
	if(minTheta < M_PI / 2.0f && maxTheta >= 4.0f * M_PI / 3.0f)
	{
		shiftTheta = 2.0f * M_PI - maxTheta + 1.0f; //The 1.0f is just for stability.
		sumTheta = 0.0f;
		for(const Line& line: lines)
		{
			const float theta = line.theta;
			sumTheta += fmodf(theta + shiftTheta,2.0f * M_PI);
		}
	}

	return fmodf(sumTheta / static_cast<float>(lines.size()) - shiftTheta,2.0f * M_PI);
}

float DifferenceTheta(const float theta1,const float theta2)
{
	//Find angle difference while taking wrapping into account.
	return std::min(fabsf(theta1 - theta2),
					std::min(theta1,theta2) + 2.0f * static_cast<float>(M_PI) - std::max(theta1,theta2));
}

bool IntersectLines(const Line& line1,const Line& line2,float& intersectionX,float& intersectionY)
{
	//Based on Image Processing: The Fundamentals Example B5.69 page 521.

	if(line1.theta == line2.theta)
		return false; //Lines are parallel. Never intersect.

	const float cosTheta1 = cosf(line1.theta);
	const float sinTheta1 = sinf(line1.theta);

	const float cosTheta2 = cosf(line2.theta);
	const float sinTheta2 = sinf(line2.theta);

	const float sinDiff = sinf(line2.theta - line1.theta);

	intersectionX = (line1.rho * sinTheta2 - line2.rho * sinTheta1) / sinDiff;
	intersectionY = (line1.rho * cosTheta2 - line2.rho * cosTheta1) / -sinDiff;

	return true;
}

