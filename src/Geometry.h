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

float MeanTheta(const std::vector<Line>& lines);
float DifferenceTheta(const float theta1,const float theta2);

bool IntersectLines(const Line& line1,const Line& line2,float& intersectionX,float& intersectionY);

#endif

