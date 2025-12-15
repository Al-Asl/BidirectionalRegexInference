#include <util.hpp>

#include <fstream>

bool rei::readStream(std::istream& stream, std::vector<std::string>& pos, std::vector<std::string>& neg) {
    std::string line;

    while (line != "++") {
        getline(stream, line);
        if (stream.eof()) {
            printf("Unable to find \"++\" for positive words");
            printf("\nPlease check the input file.\n");
            return false;
        }
    }

    getline(stream, line);

    while (line != "--") {
        std::string word = "";
        for (auto c : line) if (c != ' ' && c != '"') word += c;
        pos.push_back(word);
        getline(stream, line);
        if (line != "--" && stream.eof()) {
            printf("Unable to find \"--\" for negative words");
            printf("\nPlease check the input file.\n");
            return false;
        }
    }

    while (getline(stream, line)) {
        std::string word = "";
        for (auto c : line) if (c != ' ' && c != '"') word += c;
        for (auto& p : pos) {
            if (word == p) {
                printf("\"%s\" is in both Pos and Neg examples", word.c_str());
                printf("\nPlease check the input file, and remove one of those.\n");
                return false;
            }
        }
        neg.push_back(word);
    }

    return true;
}

// Reading the input file
bool rei::readFile(const std::string& fileName, std::vector<std::string>& pos, std::vector<std::string>& neg) {

    std::ifstream textFile(fileName);

    if (textFile.is_open()) {

        if (!readStream(textFile, pos, neg)) return false;

        textFile.close();

        return true;
    }

    printf("Unable to open the file");
    return false;

}