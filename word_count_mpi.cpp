// mpi_tokenization_frequency_parallel.cpp
#include <mpi.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>

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

// Функція токенізації рядка та підрахунку частоти слів
// Ця функція розбиває рядок на слова (токени), видаляє пунктуацію,
// переводить їх до нижнього регістру і підраховує їх кількість.
unordered_map<string, int> tokenize_and_count(const string &line) {
    unordered_map<string, int> freq;
    istringstream iss(line);
    string word;
    while (iss >> word) {
        word = remove_punctuation(word);
        word = to_lower(word);
        if (!word.empty()) {
            freq[word]++;
        }
    }
    return freq;
}

// Функція для об'єднання двох частотних карт
void merge_maps(unordered_map<string, int>& dest, const unordered_map<string, int>& src) {
    for (const auto &entry : src) {
        dest[entry.first] += entry.second;
    }
}

int main(int argc, char* argv[]) {
    // Ініціалізація середовища MPI
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);  // Отримання унікального номера поточного процесу
    MPI_Comm_size(MPI_COMM_WORLD, &size);  // Отримання загальної кількості процесів
    
    double start_time = MPI_Wtime();
    
    // Вектор для зберігання рядків, які отримає кожен процес
    vector<string> local_lines;
    
    if (rank == 0) {
        // Процес 0: зчитування вхідного файлу і розбиття тексту на рядки
        if (argc < 2) {
            cerr << "Використання: " << argv[0] << " <ім'я_файлу>" << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        ifstream file(argv[1]);
        if (!file) {
            cerr << "Помилка при відкритті файлу." << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        vector<string> all_lines;
        string line;
        while (getline(file, line)) {
            all_lines.push_back(line);
        }
        file.close();
        
        int total_lines = all_lines.size();
        // Розподілення рядків між процесами
        vector<int> counts(size, total_lines / size);
        int rem = total_lines % size;
        for (int i = 0; i < size; i++) {
            if (i < rem)
                counts[i]++;
        }
        // Обчислення зсувів (offset) для кожного процесу
        vector<int> offsets(size, 0);
        for (int i = 1; i < size; i++) {
            offsets[i] = offsets[i - 1] + counts[i - 1];
        }
        
        // Процес 0 зберігає свою частину рядків
        local_lines.assign(all_lines.begin(), all_lines.begin() + counts[0]);
        
        // Відсилаємо дані іншим процесам
        for (int proc = 1; proc < size; proc++) {
            int count = counts[proc];
            MPI_Send(&count, 1, MPI_INT, proc, 0, MPI_COMM_WORLD);
            for (int j = 0; j < count; j++) {
                const string &s = all_lines[offsets[proc] + j];
                int len = s.size();
                MPI_Send(&len, 1, MPI_INT, proc, 0, MPI_COMM_WORLD);
                if (len > 0) {
                    MPI_Send(s.c_str(), len, MPI_CHAR, proc, 0, MPI_COMM_WORLD);
                }
            }
        }
    } else {
        // Процеси з rank ≠ 0 отримують свої рядки від процесу 0
        int count;
        MPI_Recv(&count, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for (int i = 0; i < count; i++) {
            int len;
            MPI_Recv(&len, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            string s;
            if (len > 0) {
                char* buffer = new char[len + 1];
                MPI_Recv(buffer, len, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                buffer[len] = '\0';
                s = string(buffer);
                delete[] buffer;
            }
            local_lines.push_back(s);
        }
    }
    
    // Паралельна токенізація та підрахунок частоти слів здійснюється за рахунок того,
    // що кожен MPI процес одночасно обробляє свій набір рядків.
    // Кожен процес послідовно виконує токенізацію своїх рядків і підрахунок частоти.
    unordered_map<string, int> local_frequency;
    for (size_t i = 0; i < local_lines.size(); i++) {
        unordered_map<string, int> line_freq = tokenize_and_count(local_lines[i]);
        merge_maps(local_frequency, line_freq);
    }
    
    // Серіалізація локальної частотної карти у рядок для передачі
    ostringstream oss;
    for (const auto &p : local_frequency) {
        oss << p.first << " " << p.second << "\n";
    }
    string local_serial = oss.str();
    
    if (rank == 0) {
        // Процес 0 отримує результати від інших процесів та об'єднує їх
        unordered_map<string, int> final_frequency = local_frequency;
        for (int proc = 1; proc < size; proc++) {
            int len;
            MPI_Recv(&len, 1, MPI_INT, proc, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            string proc_serial;
            if (len > 0) {
                char* buf = new char[len + 1];
                MPI_Recv(buf, len, MPI_CHAR, proc, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                buf[len] = '\0';
                proc_serial = string(buf);
                delete[] buf;
            }
            istringstream iss(proc_serial);
            string word;
            int count;
            while (iss >> word >> count) {
                final_frequency[word] += count;
            }
        }
        // Вивід фінальних результатів
        cout << "Частота входження слів:" << endl;
        for (const auto &entry : final_frequency) {
            cout << entry.first << ": " << entry.second << endl;
        }
    } else {
        // Інші процеси надсилають свою серіалізовану частотну карту процесу 0
        int len = local_serial.size();
        MPI_Send(&len, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        if (len > 0) {
            MPI_Send(local_serial.c_str(), len, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
        }
    }
    
    double end_time = MPI_Wtime();
    if (rank == 0) {
        cout << "Затрачений час: " << (end_time - start_time) * 1000<< " мс." << endl;
    }
    
    MPI_Finalize();
    return 0;
}
