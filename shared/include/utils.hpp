#ifndef UTILS_HPP
#define UTILS_HPP

#include <vector>
#include <sstream>
#include <iostream>
#include <charconv>

namespace rei
{
	bool readStream(std::istream& stream, std::vector<std::string>& pos, std::vector<std::string>& neg);

	// Reading the input file
	bool readFile(const std::string& fileName, std::vector<std::string>& pos, std::vector<std::string>& neg);

	template <typename ArgType>
	bool parse_number_arg(const char* arg, ArgType& value) {

		std::string_view arg_view(arg);

		auto [ptr, ec] = std::from_chars(arg_view.data(), arg_view.data() + arg_view.size(), value);

		if (ec == std::errc::result_out_of_range) {
			std::cerr << "Error: The number is too large!\n";
			return false;
		}
		else if (ec == std::errc::invalid_argument) {
			std::cerr << "Error: Not a valid number.\n";
			return false;
		}

		return true;
	}

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

	OperationsCount countOpreations(const std::string& pattern);

	int calculateCost(const std::string& pattren, const unsigned short* costFunc);
}

#endif //end UTILS_HPP