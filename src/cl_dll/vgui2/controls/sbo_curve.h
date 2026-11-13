//========= SBO VGUI2 controls: curve math =========//
//
// Purpose: the curve of a curve control (sbo_curve_editor.h) as plain math, without VGUI2 or engine headers, so the
//          tools that use the curve (the color correction editor's Curves operation, later the particle editor) and
//          the control draw exactly the same line. A monotone cubic through the points (Fritsch-Carlson): it never
//          overshoots between two points, so a curve that only rises never dips.
//=================================================================//

#ifndef SBO_VGUI2_CONTROLS_SBO_CURVE_H
#define SBO_VGUI2_CONTROLS_SBO_CURVE_H

#include <math.h>

namespace sboctl
{
struct CurvePoint
{
	float x, y; //!< 0 .. 1 each
};

constexpr int CURVE_MAX_POINTS = 16;

//! The curve's value at x. p: n points (1 .. CURVE_MAX_POINTS) sorted by x, x strictly rising. Left of the first
//! point the first point's y, right of the last the last's. The result is not clamped.
inline float EvalMonotoneCurve(const CurvePoint *p, int n, float x)
{
	if (!p || n <= 0)
		return x;
	if (n > CURVE_MAX_POINTS)
		n = CURVE_MAX_POINTS;
	if (n == 1 || x <= p[0].x)
		return p[0].y;
	if (x >= p[n - 1].x)
		return p[n - 1].y;

	// secant slopes, then the tangents (Fritsch-Carlson)
	float d[CURVE_MAX_POINTS], m[CURVE_MAX_POINTS];
	for (int i = 0; i < n - 1; i++)
	{
		const float dx = p[i + 1].x - p[i].x;
		d[i] = dx > 1e-6f ? (p[i + 1].y - p[i].y) / dx : 0.0f;
	}
	m[0] = d[0];
	m[n - 1] = d[n - 2];
	for (int i = 1; i < n - 1; i++)
		m[i] = (d[i - 1] * d[i] <= 0.0f) ? 0.0f : 0.5f * (d[i - 1] + d[i]);
	for (int i = 0; i < n - 1; i++)
	{
		if (d[i] == 0.0f)
		{
			m[i] = m[i + 1] = 0.0f;
			continue;
		}
		const float a = m[i] / d[i], b = m[i + 1] / d[i];
		const float s = a * a + b * b;
		if (s > 9.0f)
		{
			const float t = 3.0f / sqrtf(s);
			m[i] = t * a * d[i];
			m[i + 1] = t * b * d[i];
		}
	}

	int k = 0;
	while (k < n - 2 && x > p[k + 1].x)
		k++;
	const float h = p[k + 1].x - p[k].x;
	if (h <= 1e-6f)
		return p[k + 1].y;
	const float t = (x - p[k].x) / h, t2 = t * t, t3 = t2 * t;
	return (2 * t3 - 3 * t2 + 1) * p[k].y + (t3 - 2 * t2 + t) * h * m[k] + (-2 * t3 + 3 * t2) * p[k + 1].y +
	       (t3 - t2) * h * m[k + 1];
}
} // namespace sboctl

#endif // SBO_VGUI2_CONTROLS_SBO_CURVE_H
