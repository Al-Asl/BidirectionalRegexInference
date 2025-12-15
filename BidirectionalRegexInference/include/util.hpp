#ifndef UTIL_HPP
#define UTIL_HPP

#include <vector>
#include <sstream>
#include <iostream>

namespace rei
{
	// Reading the input stream
	bool readStream(std::istream& stream, std::vector<std::string>& pos, std::vector<std::string>& neg);

	// Reading the input file
	bool readFile(const std::string& fileName, std::vector<std::string>& pos, std::vector<std::string>& neg);

	class OperationsCount {
	public:
		int alpha = 0;
		int question = 0;
		int star = 0;
		int concat = 0;
		int alternation = 0;
		int intersection = 0;

		OperationsCount() = default;
	};

	// return the number of each operation in the regex pattern
	OperationsCount countOpreations(const std::string& pattern);
}

#endif //end UTIL_HPP