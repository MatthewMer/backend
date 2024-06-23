#include "pch.h"
#include "framework.h"

#include "data_io.h"

#include "logger.h"
#include "helper_functions.h"

#include <fstream>
#include <string>
#include <filesystem>

using namespace std;
namespace fs = filesystem;

namespace Backend {
    namespace FileIO {
        

        bool check_and_create_path(const string& _path_rel) {
            auto path = Helpers::split_string(_path_rel, "/");
            bool exists = true;

            for (size_t i = 0; i < path.size(); i++) {
                string sub_path = "";
                for (int j = 0; j <= i; j++) {
                    sub_path += path[j] + "/";
                }

                if (!fs::exists(sub_path) || !fs::is_directory(sub_path)) {
                    fs::create_directory(sub_path);
                    exists = false;
                }
            }

            return exists;
        }

        bool check_and_create_file(const string& _path_to_file) {
            if (!fs::exists(_path_to_file)) {
                ofstream(_path_to_file).close();
                return false;
            } else {
                return true;
            }
        }

        bool check_file(const string& _path_to_file) {
            return fs::exists(_path_to_file);
        }

        bool check_file_exists(const string& _path_to_file) {
            return fs::exists(_path_to_file);
        }

        std::string get_current_path() {
            vector<string> current_path_vec = Helpers::split_string(fs::current_path().string(), "\\");
            string current_path = "";
            for (const auto& n : current_path_vec) {
                current_path += n + "/";
            }

            return current_path;
        }

        void get_files_in_path(vector<string>& _input, const string& _path_rel) {
            _input.clear();

            for (const auto& n : fs::directory_iterator(_path_rel)) {
                if (n.is_directory()) { continue; }
                _input.emplace_back(n.path().generic_string());
            }
        }

        bool write_to_debug_log(const string& _output, const string& _file_path, const bool& _rewrite) {
            return write_data({ _output }, _file_path, _rewrite);
        }


        bool read_data(vector<string>& _input, const string& _file_path) {
            check_and_create_file(_file_path);

            ifstream is(_file_path, ios::beg);
            if (!is) {
                LOG_WARN("[emu] Couldn't open ", _file_path);
                return false;
            }

            string line;
            _input.clear();
            while (getline(is, line)) {
                _input.push_back(line);
            }

            is.close();
            return true;
        }

        bool read_data(std::vector<char>& _input, const std::string& _file_path) {
            if (check_file(_file_path)) {
                ifstream is(_file_path, ios::beg | ios::binary);
                if (!is) {
                    LOG_WARN("[emu] Couldn't open ", _file_path);
                    return false;
                }

                _input = vector<char>(istreambuf_iterator<char>(is), istreambuf_iterator<char>());
                if (_input.size() > 0) {
                    return true;
                } else {
                    LOG_ERROR("[emu] file ", _file_path, " damaged");
                    return false;
                }
            } else {
                LOG_ERROR("[emu] file ", _file_path, " does not exist");
                return false;
            }
        }

        bool write_data(const vector<string>& _output, const string& _file_path, const bool& _rewrite) {
            check_and_create_file(_file_path);

            ofstream os(_file_path, (_rewrite ? ios::trunc : ios::app));
            if (!os.is_open()) {
                LOG_WARN("[emu] Couldn't write ", _file_path);
                return false;
            }

            for (const auto& n : _output) {
                os << n << endl;
            }

            os.close();
            return true;
        }

        bool write_data(const vector<char>& _output, const string& _file_path, const bool& _rewrite) {
            check_and_create_file(_file_path);

            ofstream os(_file_path, (_rewrite ? ios::trunc : ios::app) | ios::binary);
            if (!os.is_open()) {
                LOG_WARN("[emu] Couldn't write ", _file_path);
                return false;
            }

            for (const auto& n : _output) {
                os << n;
            }

            os.close();
            return true;
        }
    }
}