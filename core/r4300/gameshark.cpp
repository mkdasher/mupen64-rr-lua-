#include "gameshark.h"

#include <string>
#include <vector>
#include <core/memory/memory.h>

#include "r4300.h"
#include "shared/helpers/StringHelpers.h"

namespace Gameshark
{
	std::vector<std::shared_ptr<Gameshark::Script>> scripts;
}

void Gameshark::Script::execute()
{
	if (!m_resumed)
	{
		return;
	}

	bool execute = true;
	for (auto instruction : m_instructions)
	{
		if (execute)
		{
			execute = std::get<1>(instruction)();
		} else if (!std::get<0>(instruction))
		{
			execute = true;
		}
	}
}

std::optional<std::shared_ptr<Gameshark::Script>> Gameshark::Script::compile(const std::string& code)
{
	auto script = std::make_shared<Script>();

	auto lines = split_string(code, "\n");

	bool serial = false;
	size_t serial_count = 0;
	size_t serial_offset = 0;
	size_t serial_diff = 0;

	for (const auto& line : lines)
	{
		if (line[0] == '$' || line[0] == '-' || line.size() < 13)
		{
			printf("[GS] Line skipped\n");
			continue;
		}

		auto opcode = line.substr(0, 2);

		uint32_t address = std::stoul(line.substr(2, 6), nullptr, 16);
		uint32_t val;
		if (line.substr(8, 1) == " ")
		{
			val = std::stoul(line.substr(9, 4), nullptr, 16);
		} else
		{
			val = std::stoul(line.substr(10, 4), nullptr, 16);
		}

		if (serial)
		{
			printf("[GS] Compiling %u serial byte writes...\n", serial_count);

			for (size_t i = 0; i < serial_count; ++i)
			{
				// Madghostek: warning, assumes that serial codes are writing bytes, which seems to match pj64
				// Madghostek: if not, change WB to WW
				script->m_instructions.emplace_back(std::make_tuple(false, [=]
				{
					StoreRDRAMSafe<uint8_t>(address + serial_offset * i, val + serial_diff * i);
					return true;
				}));
			}
			serial = false;
			continue;
		}


		if (opcode == "80" || opcode == "A0")
		{
			// Write byte
			script->m_instructions.emplace_back(std::make_tuple(false, [=]
			{
				StoreRDRAMSafe<uint8_t>(address, val & 0xFF);
				return true;
			}));
		} else if (opcode == "81" || opcode == "A1")
		{
			// Write word
			script->m_instructions.emplace_back(std::make_tuple(false, [=]
			{
				StoreRDRAMSafe<uint16_t>(address, val);
				return true;
			}));
		} else if (opcode == "88")
		{
			// Write byte if GS button pressed
			script->m_instructions.emplace_back(std::make_tuple(false, [address, val, &script]
			{
				if (get_gs_button())
				{
					StoreRDRAMSafe<uint8_t>(address, val & 0xFF);
				}
				return true;
			}));
		} else if (opcode == "89")
		{
			// Write word if GS button pressed
			script->m_instructions.emplace_back(std::make_tuple(false, [address, val, &script]
			{
				if (get_gs_button())
				{
					StoreRDRAMSafe<uint16_t>(address, val);
				}
				return true;
			}));
		} else if (opcode == "D0")
		{
			// Byte equality comparison
			script->m_instructions.emplace_back(std::make_tuple(true, [=]
			{
				return LoadRDRAMSafe<uint8_t>(address) == val & 0xFF;
			}));
		} else if (opcode == "D1")
		{
			// Word equality comparison
			script->m_instructions.emplace_back(std::make_tuple(true, [=]
			{
				return LoadRDRAMSafe<uint16_t>(address) == val;
			}));
		} else if (opcode == "D2")
		{
			// Byte inequality comparison
			script->m_instructions.emplace_back(std::make_tuple(true, [=]
			{
				return LoadRDRAMSafe<uint8_t>(address) != val & 0xFF;
			}));
		} else if (opcode == "D3")
		{
			// Word inequality comparison
			script->m_instructions.emplace_back(std::make_tuple(true, [=]
			{
				return LoadRDRAMSafe<uint16_t>(address) != val;
			}));
		} else if (opcode == "50")
		{
			// Enter serial mode, which writes multiple bytes for example
			serial = true;
			serial_count = (address & 0xFF00) >> 8;
			serial_offset = address & 0xFF;
			serial_diff = val;
		} else
		{
			printf("[GS] Illegal instruction %s\n", opcode.c_str());
			return std::nullopt;
		}
	}

	script->m_code = std::string(code);
	return std::make_optional(script);
}

void Gameshark::execute()
{
	for (auto script : scripts)
	{
		script->execute();
	}
}
