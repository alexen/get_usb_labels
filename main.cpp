#include <iostream>
#include <string>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>

#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <boost/algorithm/string/trim.hpp>

// Функция декодирования hex-escapes (\x20)
std::string decode_hex_escapes(const std::string& input) {
     std::string result;
     result.reserve(input.length());
     for (size_t i = 0; i < input.length(); ++i) {
          if (input[i] == '\\' && i + 3 < input.length() && input[i + 1] == 'x') {
               std::string hex_str = input.substr(i + 2, 2);
               try {
                    char ch = static_cast<char>(std::stoul(hex_str, nullptr, 16));
                    result += ch;
                    i += 3;
                    continue;
               } catch (...) {}
          }
          result += input[i];
     }
     return result;
}

std::string trim(const std::string& str) {
     size_t first = str.find_first_not_of(" \t\r\n");
     if (std::string::npos == first) return "";
     size_t last = str.find_last_not_of(" \t\r\n");
     return str.substr(first, (last - first + 1));
}

std::string get_readable_size(const std::string& dev_name) {
     std::string parent_name = dev_name;
     while (!parent_name.empty() && std::isdigit(parent_name.back())) {
          parent_name.pop_back();
     }

     std::ifstream size_file("/sys/class/block/" + parent_name + "/size");
     long long sectors = 0;
     if (!(size_file >> sectors)) return "?? GB";

     double bytes = sectors * 512.0;
     const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};
     int s = 0;
     while (bytes >= 1024 && s < 4) {
          s++;
          bytes /= 1024;
     }

     std::ostringstream oss;
     oss << std::fixed << std::setprecision(1) << bytes << " " << suffixes[s];
     return oss.str();
}

std::string get_device_model(const std::string& dev_name) {
     std::string parent_name = dev_name;
     while (!parent_name.empty() && std::isdigit(parent_name.back())) {
          parent_name.pop_back();
     }

     std::string base_path = "/sys/class/block/" + parent_name + "/device/";
     std::string vendor, model;

     std::ifstream vendor_file(base_path + "vendor");
     std::getline(vendor_file, vendor);

     std::ifstream model_file(base_path + "model");
     std::getline(model_file, model);

     vendor = trim(vendor);
     model = trim(model);

     std::string info;
     if (!vendor.empty()) info += vendor + " ";
     if (!model.empty()) info += model;

     return info.empty() ? "Неизвестное устройство" : info;
}

// Разрешение симлинков через POSIX realpath
std::string resolve_linux_symlink(const std::string& path_str) {
     char resolved[PATH_MAX];
     if (realpath(path_str.c_str(), resolved) != nullptr) {
          std::string res(resolved);
          size_t last_slash = res.find_last_of('/');
          if (last_slash != std::string::npos) {
               return res.substr(last_slash + 1);
          }
     }
     return "";
}

// Получение метки для конкретного устройства (например, sdc1)
std::string find_label_for_device(const std::string& target_dev) {
     const boost::filesystem::path target_label_dir = "/dev/disk/by-label";
     boost::system::error_code ec;

     if (boost::filesystem::exists(target_label_dir, ec) && !ec) {
          boost::filesystem::directory_iterator entry(target_label_dir, ec);
          boost::filesystem::directory_iterator end;

          while (!ec && entry != end) {
               boost::filesystem::file_status sym_stat = entry->symlink_status(ec);
               if (!ec && sym_stat.type() == boost::filesystem::symlink_file) {
                    std::string dev_name = resolve_linux_symlink(entry->path().string());
                    if (dev_name == target_dev) {
                         return decode_hex_escapes(entry->path().filename().string());
                    }
               }
               entry.increment(ec);
          }
     }
     return "";
}

// Получение информации о диске по его UUID
std::string get_disk_info_by_uuid(const std::string& uuid) {
     const std::string uuid_dir = "/dev/disk/by-uuid/";
     std::string full_uuid_path = uuid_dir + uuid;
     boost::system::error_code ec;

     // Проверяем, существует ли такой UUID в системе
     if (!boost::filesystem::exists(full_uuid_path, ec) || ec) {
          return "Ошибка: UUID " + uuid + " не найден в системе.";
     }

     // Определяем имя устройства по симлинку (например, "sdc1")
     std::string dev_name = resolve_linux_symlink(full_uuid_path);
     if (dev_name.empty()) {
          return "Ошибка: Не удалось разрешить устройство для UUID " + uuid;
     }

     // Собираем железные данные
     std::string model = get_device_model(dev_name);
     std::string size = get_readable_size(dev_name);
     std::string label = find_label_for_device(dev_name);

     // Форматируем строку вывода
     std::ostringstream oss;
     oss << model << ", " << size;
     if (!label.empty()) {
          oss << " (" << label << ")";
     }

     return oss.str();
}

int main(int argc, char* argv[]) {
     // Проверяем, передан ли хотя бы один аргумент
     if (argc < 2) {
          std::cout << "Использование: " << argv[0] << " [UUID_1] [UUID_2] [UUID_3] ...\n";
          std::cout << "Пример: " << argv[0] << " 7BF7-B6CC 1A2B-3C4D\n";
          return 1;
     }

     // Будем отслеживать, произошла ли ошибка хотя бы для одного UUID
     bool total_success = true;

     // Последовательно перебираем все переданные аргументы
     for (int i = 1; i < argc; ++i) {
          std::string input_uuid = argv[i];

          // Приводим UUID к верхнему регистру
          std::transform(input_uuid.begin(), input_uuid.end(), input_uuid.begin(), [](unsigned char c) {
               return std::toupper(c);
          });

          std::string result = get_disk_info_by_uuid(input_uuid);

          // Выводим UUID и результат его обработки
          std::cout << input_uuid << " -> " << result << "\n";

          // Если внутри строки результата есть слово "Ошибка", фиксируем сбой
          if (result.rfind("Ошибка", 0) == 0) {
               total_success = false;
          }
     }

     // Возвращаем 0, если все диски нашлись успешно, или 1, если была хоть одна ошибка
     return total_success ? 0 : 1;
}
