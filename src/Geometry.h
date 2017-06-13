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

#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <vector>

struct Line
{
	float theta; //Radians
	float rho;
};

struct Point
{
	float x;
	float y;
};

Point operator*(const Point& lhs,const Point& rhs);

float MeanTheta(const std::vector<Line>& lines);
float DifferenceTheta(const float theta1,const float theta2);

bool IntersectLines(const Line& line1,const Line& line2,float& intersectionX,float& intersectionY);

#endif

