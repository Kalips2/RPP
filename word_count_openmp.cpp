// parallel_tokenization_frequency.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <omp.h>

using namespace std;

// Функція для приведення рядка до нижнього регістру
string to_lower(const string &s) {
    string result = s;
    transform(result.begin(), result.end(), result.begin(),
              [](unsigned char c) { return tolower(c); });
    return result;
}

// Функція для видалення знаків пунктуації з рядка
string remove_punctuation(const string &s) {
    string result;
    for (char c : s) {
        if (!ispunct(static_cast<unsigned char>(c)))
            result.push_back(c);
    }
    return result;
}

// Функція токенізації рядка – розбиває рядок на окремі слова
vector<string> tokenize_line(const string &line) {
    vector<string> tokens;
    istringstream iss(line);
    string word;
    while (iss >> word) {
        // Видаляємо знаки пунктуації та переводимо слово до нижнього регістру
        word = remove_punctuation(word);
        word = to_lower(word);
        if (!word.empty())
            tokens.push_back(word);
    }
    return tokens;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Використання: " << argv[0] << " <ім'я_файлу> [кількість_потоків]" << endl;
        return 1;
    }

    // Якщо задано параметр для кількості потоків, встановлюємо його
    int user_threads = omp_get_max_threads();
    if (argc >= 3) {
        user_threads = stoi(argv[2]);
        omp_set_num_threads(user_threads);
    }
    
    // Зчитування файлу та розбиття тексту на рядки
    ifstream file(argv[1]);
    if (!file) {
        cerr << "Помилка при відкритті файлу." << endl;
        return 1;
    }
    vector<string> lines;
    string line;
    while (getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    
    int n_lines = lines.size();
    // Встановлюємо кількість потоків, яка зараз використовується
    int num_threads = user_threads;
    // Створюємо вектор для зберігання локальних карт частот для кожного потоку
    vector<unordered_map<string, int>> local_maps(num_threads);

    // Замір часу початку обчислень (паралельної частини)
    double start_time = omp_get_wtime();

    // Паралельна область:
    // Директива "omp parallel for schedule(dynamic)" розподіляє обробку рядків (токенізацію та підрахунок) між потоками.
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < n_lines; i++) {
        int tid = omp_get_thread_num();
        // Токенізуємо рядок
        vector<string> tokens = tokenize_line(lines[i]);
        // Одразу підраховуємо частоту входження кожного слова для цього рядка у локальному словнику потоку
        for (const auto &word : tokens) {
            local_maps[tid][word]++;
        }
    }

    // Об'єднуємо локальні карти частот у глобальну карту
    unordered_map<string, int> global_frequency;
    for (const auto &local_map : local_maps) {
        for (const auto &entry : local_map) {
            global_frequency[entry.first] += entry.second;
        }
    }

    // Замір часу завершення паралельної частини
    double end_time = omp_get_wtime();

    // Вивід результатів – частоти входження кожного слова
    cout << "Частота входження слів:" << endl;
    for (const auto &p : global_frequency) {
        cout << p.first << ": " << p.second << endl;
    }

    cout << "Затрачений час: " << (end_time - start_time) * 1000 << " ms." << endl;
    
    return 0;
}
