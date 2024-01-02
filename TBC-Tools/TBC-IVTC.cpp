#include "TBC-IVTC.h"

#include <vector>

using namespace std;

vector<uint32_t>	pulldowns;
uint32_t			pulldownFieldCount	= 0;
uint32_t			itCount				= 0;

void setup_pulldown(string pulldown)
{
	size_t pos = 0;
	string token;

	pulldown.append(":");
	while ((pos = pulldown.find(":")) != string::npos)
	{
		token = pulldown.substr(0, pos);
		uint32_t pull = stoi(token);
		pulldowns.push_back(pull);
		pulldownFieldCount += pull;
		pulldown.erase(0, pos + 1);
	}

	if (pulldownFieldCount % 2 == 1)
	{
		pulldownFieldCount *= 2;
		itCount = pulldowns.size() * 2;
	}
	else
	{
		itCount = pulldowns.size();
	}

	set_field_accumulation(pulldownFieldCount);
}

// Apply pulldown on fields
void pulldown_fields(uint32_t phase)
{
	uint32_t fieldIdx = 0;
	uint32_t fieldOutIdx = 0;

	for (uint32_t i = 0; i < itCount; i++)
	{
		uint32_t offset = 0;
		uint32_t pull = pulldowns[i % pulldowns.size()];

		if (pull % 2 == 1 && fieldIdx % 2 == 1) offset = 1;

		route_field((phase + fieldIdx + offset + 0) % pulldownFieldCount, fieldOutIdx++);
		route_field((phase + fieldIdx + offset + 1) % pulldownFieldCount, fieldOutIdx++);

		fieldIdx += pull;
	}
}