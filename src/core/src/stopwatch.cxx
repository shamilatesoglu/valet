#include "valet/stopwatch.hxx"

namespace valet::util
{
Stopwatch::Stopwatch()
{
	reset();
}

void Stopwatch::reset()
{
	start_time = std::chrono::high_resolution_clock::now();
}

double Stopwatch::elapsed() const
{
	auto end_time = std::chrono::high_resolution_clock::now();
	auto elapsed_time =
	    std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
	return elapsed_time.count() / 1000000.0;
}

std::string Stopwatch::elapsed_str() const
{
	double seconds = elapsed();
	if (seconds < 1e-6) {
		return std::to_string(seconds * 1e9) + "ns";
	} else if (seconds < 1e-3) {
		return std::to_string(seconds * 1e6) + "us";
	} else if (seconds < 1) {
		return std::to_string(seconds * 1e3) + "ms";
	} else {
		return std::to_string(seconds) + "s";
	}
}

} // namespace valet::util