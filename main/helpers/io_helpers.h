#pragma once

#include <filesystem>
#include <Windows.h>
#include <string>
#include <vector>

typedef struct
{
	std::vector<uint8_t> data;
	size_t offset;
} t_buffer_io;

std::vector<std::string> get_files_with_extension_in_directory(
	const std::string &directory, const std::string &extension);

std::vector<std::string> get_files_in_subdirectories(
	const std::string& directory);

std::string strip_extension(const std::string& path);
std::wstring strip_extension(const std::wstring &path);
std::wstring get_extension(const std::wstring &path);

bool write_file_buffer(const std::filesystem::path& path, const std::vector<uint8_t>& buffer);
std::vector<uint8_t> read_file_buffer(const std::filesystem::path& path);

void copy_to_clipboard(HWND owner, const std::string &str);

std::wstring get_desktop_path();

void bwrite(t_buffer_io* buffer, void* val, size_t len);
void bread(t_buffer_io* buffer, void* val, size_t len);
void memread(char** src, void* dest, unsigned int len);
