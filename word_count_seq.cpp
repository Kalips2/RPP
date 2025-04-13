#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <cctype>

std::string to_lower(const std::string &s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return result;
}

std::string remove_punctuation(const std::string &s) {
    std::string result;
    for (char c : s) {
        if (!std::ispunct(static_cast<unsigned char>(c)))
            result.push_back(c);
    }
    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        return 1;
    }
    
    auto start = std::chrono::steady_clock::now();

    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "Error opening file." << std::endl;
        return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string text = buffer.str();
    file.close();

    std::unordered_map<std::string, int> word_count;
    std::istringstream iss(text);
    std::string word;
    while (iss >> word) {
        word = remove_punctuation(word);
        word = to_lower(word);
        if (!word.empty())
            ++word_count[word];
    }

    for (const auto &p : word_count)
        std::cout << p.first << ": " << p.second << std::endl;

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Elapsed time: " << elapsed.count() * 1000 << " ms" << std::endl;
    
    return 0;
}
