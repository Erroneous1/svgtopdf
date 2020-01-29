#include "stdafx.h"
#include "SvgConverter.h"
#include <iostream>

void SvgConverter::error_handler(HPDF_STATUS   error_no,
	HPDF_STATUS   detail_no,
	void         *user_data)
{
	printf_s("HPDF ERROR: error_no=%04X, detail_no=%u\n", (HPDF_UINT)error_no,
		(HPDF_UINT)detail_no);

}

SvgConverter::SvgConverter(std::string fileName) {
	this->fileName = fileName;
	this->g_image = nsvgParseFromFile(fileName.c_str(), "px", 96.0f);
}
SvgConverter::SvgConverter() {
	this->fileName.clear();
	this->g_image = nullptr;
}
bool SvgConverter::isLoaded() {
	return this->g_image;
}
bool SvgConverter::loadFromFile(std::string fileName) {
	if (g_image)
		nsvgDelete(g_image);

	g_image = nsvgParseFromFile(fileName.c_str(), "px", 96.0f); // returns svg image as paths
	if (!g_image) {
		std::cerr << "Could not open SVG image." << std::endl;;
		return false;
	}
	return true;
}

bool SvgConverter::convertToPDF(std::string fileName) {
	if (fileName.empty() || !isLoaded()) return false;

	HPDF_Doc pdf = HPDF_New(error_handler, NULL);
	if (!pdf) return false;

	HPDF_Page page = HPDF_AddPage(pdf);
	float width = this->g_image->width;
	float height = this->g_image->height;
	Vector2f startPoint = { 0, height};

	HPDF_Page_SetWidth(page, width);
	HPDF_Page_SetHeight(page, height);
	HPDF_Page_SetLineWidth(page, 0.1f); //initializing page

	for (NSVGshape * shape = g_image->shapes; shape != NULL; shape = shape->next) {
		if (!(shape->flags & NSVG_FLAGS_VISIBLE)) continue; //pass invisible shapes

		float r = (float)((shape->fill.color >> 16) & 0xFF) / 255.0f;
		float g = (float)((shape->fill.color >> 8) & 0xFF) / 255.0f;
		float b = (float)((shape->fill.color) & 0xFF) / 255.0f;
		float gray = (r + g + b) / 3.0f; 
		gray = (gray < 0.5f) ? 0 : 1.0f;

		HPDF_Page_SetGrayFill(page, gray); // sets the filling color

		for (NSVGpath * path = shape->paths; path != NULL; path = path->next ){
			// drawing each path in shape to pdf file
			pdfPath(page, path->pts, path->npts, path->closed, 0.1f, shape->fill.type != NSVG_PAINT_NONE, startPoint);
		}
		if (shape->fill.type != NSVG_PAINT_NONE) {
			if (shape->fillRule == NSVGfillRule::NSVG_FILLRULE_EVENODD)
				HPDF_Page_EofillStroke(page); // fills the curr path using the even-odd rule and paints
			else HPDF_Page_FillStroke(page); // fills the curr path using the nonzero winding number rule and paints
		}
		else HPDF_Page_Stroke(page); // paints the path
	}
	HPDF_SaveToFile(pdf, fileName.c_str());
	HPDF_Free(pdf);

	return true;
}
SvgConverter::~SvgConverter() {
	if (this->g_image) nsvgDelete(g_image);
	g_image = nullptr;
}

float SvgConverter::distPtSeg(float x, float y, float px, float py, float qx, float qy)
{
	float pqx, pqy, dx, dy, d, t;
	pqx = qx - px;
	pqy = qy - py;
	dx = x - px;
	dy = y - py;
	d = pqx*pqx + pqy*pqy;
	t = pqx*dx + pqy*dy;
	if (d > 0) t /= d;
	if (t < 0) t = 0;
	else if (t > 1) t = 1;
	dx = px + t*pqx - x;
	dy = py + t*pqy - y;
	return dx*dx + dy*dy;
}


void SvgConverter::pdfcubicBez(HPDF_Page page, float x1, float y1, float x2, float y2,
	float x3, float y3, float x4, float y4,
	float /*tol*/, int /*level*/, Vector2f startPoint)
{
	HPDF_Page_MoveTo(page, x1, y1);
	HPDF_Page_CurveTo(page,
			  startPoint.x + x2 / 3.0f, startPoint.y - y2 / 3.0f,
			  startPoint.x + x3 / 3.0f, startPoint.y - y3 / 3.0f,
			  startPoint.x + x4 / 3.0f, startPoint.y - y4 / 3.0f);
}

void SvgConverter::pdfPath(HPDF_Page page, float* pts, int npts, char closed, float tol, bool bFilled, Vector2f startPoint)
{
	HPDF_Page_MoveTo(page, startPoint.x + pts[0] / 3.0f, startPoint.y - pts[1] / 3.0f); // moving to first point of bezier curve

	for (int i = 0; i < npts - 1; i += 3) {
		float* p = &pts[i * 2];
		pdfcubicBez(page, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], tol, 0, startPoint); // draw a cubic bezier curve
	}
	if (closed) HPDF_Page_LineTo(page, startPoint.x + pts[0] / 3.0f, startPoint.y - pts[1] / 3.0f);
}
