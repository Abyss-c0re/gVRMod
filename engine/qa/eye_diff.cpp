// Compare two eye PNGs. The expected disparity is recomputed here from the
// capture meta (f * ipd / depth). It does not trust a pre-baked pixel count
// except to check that Lua used the same formula.
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static bool load_meta(const char *path, double &w, double &h, double &fov, double &ipd, double &depth, double &lua_expected, bool &have_lua) {
	std::ifstream in(path);
	if (!in) {
		std::cerr << "missing meta " << path << "\n";
		return false;
	}
	std::string key;
	double value;
	while (in >> key >> value) {
		if (key == "width") w = value;
		else if (key == "height") h = value;
		else if (key == "fov_y") fov = value;
		else if (key == "ipd") ipd = value;
		else if (key == "depth") depth = value;
		else if (key == "lua_expected") {
			lua_expected = value;
			have_lua = true;
		}
	}
	return h > 0 && fov > 0 && ipd > 0 && depth > 0;
}

static bool centroid(const cv::Mat &bgr, double &cx, double &cy, int &count) {
	cv::Mat ch[3];
	cv::split(bgr, ch);
	count = 0;
	double sx = 0;
	double sy = 0;
	for (int y = 0; y < bgr.rows; ++y) {
		const unsigned char *r = ch[2].ptr<unsigned char>(y);
		const unsigned char *g = ch[1].ptr<unsigned char>(y);
		const unsigned char *b = ch[0].ptr<unsigned char>(y);
		for (int x = 0; x < bgr.cols; ++x) {
			if (r[x] > 80 && r[x] > g[x] + 40 && r[x] > b[x] + 40) {
				sx += x;
				sy += y;
				++count;
			}
		}
	}
	if (count < 10) {
		return false;
	}
	cx = sx / count;
	cy = sy / count;
	return true;
}

int main(int argc, char **argv) {
	if (argc != 4) {
		std::cerr << "usage: eye_diff left.png right.png stereo.txt\n";
		return 2;
	}
	cv::Mat left = cv::imread(argv[1], cv::IMREAD_COLOR);
	cv::Mat right = cv::imread(argv[2], cv::IMREAD_COLOR);
	if (left.empty() || right.empty()) {
		std::cerr << "failed to read eye png\n";
		return 1;
	}
	if (left.size() != right.size()) {
		std::cerr << "eye sizes differ\n";
		return 1;
	}
	double w = 0, h = 0, fov = 0, ipd = 0, depth = 0, lua_expected = 0;
	bool have_lua = false;
	if (!load_meta(argv[3], w, h, fov, ipd, depth, lua_expected, have_lua)) {
		return 1;
	}
	if (std::abs(w - left.cols) > 0.1 || std::abs(h - left.rows) > 0.1) {
		std::cerr << "png size " << left.cols << "x" << left.rows << " != meta\n";
		return 1;
	}
	cv::Mat diff;
	cv::absdiff(left, right, diff);
	cv::Scalar mean = cv::mean(diff);
	double mean_abs = (mean[0] + mean[1] + mean[2]) / 3.0;
	// A small marker on a black frame moves only a few hundred pixels, so the
	// frame-wide mean stays small. Near-zero means the two renders match.
	if (mean_abs < 0.02) {
		std::cerr << "eyes are identical, mean abs " << mean_abs << "\n";
		return 1;
	}
	double lx, ly, rx, ry;
	int lc = 0, rc = 0;
	if (!centroid(left, lx, ly, lc) || !centroid(right, rx, ry, rc)) {
		std::cerr << "red marker missing (counts " << lc << " " << rc << ")\n";
		return 1;
	}
	double dx = lx - rx;
	double dy = ly - ry;
	double f = (h * 0.5) / std::tan(fov * 0.5 * M_PI / 180.0);
	double expected = f * ipd / depth;
	std::cout << "disparity_px " << dx << " expected " << expected
		<< " dy " << dy << " marker_px " << lc << " " << rc
		<< " mean_abs " << mean_abs << "\n";
	if (have_lua && std::abs(lua_expected - expected) > 1e-4) {
		std::cerr << "lua formula mismatch " << lua_expected << " vs " << expected << "\n";
		return 1;
	}
	if (std::abs(dy) > 2.0) {
		std::cerr << "vertical shift " << dy << "\n";
		return 1;
	}
	if (std::abs(dx - expected) > 2.0) {
		std::cerr << "horizontal disparity off by " << (dx - expected) << "\n";
		return 1;
	}
	std::cout << "eye pair ok\n";
	return 0;
}
