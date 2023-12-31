﻿#include "TBC-Tools.h"
#include "TBC-IVTC.h"

#include <iostream>
#include <cstdio>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

#define TBC_TOOLS_NAME		"NebulousDev's TBC-Tools"
#define TBC_TOOLS_GITHUB	"https://github.com/NebulousDev/TBC-Tools"
#define TBC_TOOLS_VERSION	"alpha v0.3"

#define TBC_MAX_FIELDS		30

#define TBC_OUTPUT_DEFAULT_POSTFIX	"_tbct"

// Video input/output streams
string		inputStreamPath;
string		outputStreamPath;
FILE*		fpInputStream	= nullptr;
FILE*		fpOutputStream	= nullptr;

// Raw TBC Buffers (default 910x263)
uint16_t*	pPipeBuffer						= nullptr;
uint16_t*	pFields[TBC_MAX_FIELDS]			= {};
uint16_t	fieldIdxs[TBC_MAX_FIELDS * 2]	= {};
uint32_t	inputFieldCount					= 0;
uint32_t	outputFieldCount				= 0;

// Example.tbc.json files
string		tbcJSONpathIn;
string		tbcJSONpathOut;
Document	tbcJSONIn;
Document	tbcJSONOut;
Value*		pTbcFields;

// Counters
uint32_t	fieldAccCount	= 0;
uint64_t	fieldSqId		= 0;
uint64_t	fieldSqNoIn		= 0;
uint64_t	fieldSqNoOut	= 0;
uint64_t	timeout			= 0;

// Parameters
uint64_t	start			= -1;
uint64_t	phase			= 0;
bool		doTimeout		= true;
bool		doIVTC			= false;
string		pulldown;

// Set the number of fields to accumulate each proccessing period
void set_field_accumulation(uint32_t fields)
{
	fieldAccCount = fields;
}

// Read fields from the input stream into input field buffers
bool accumulate_fields(uint32_t count)
{
	uint64_t bytesRead = 0;
	uint32_t fields = 0;

	inputFieldCount = 0;

	while (fields < count)
	{
		if (doTimeout && timeout > 250000)
		{ 
			return false; // timed out
		} 

		pPipeBuffer = pFields[fields];
		bytesRead = fread(pPipeBuffer, sizeof(uint16_t), 263 * 910, fpInputStream);

		if (bytesRead > 0)
		{
			inputFieldCount++;
			fieldSqNoIn++;
			fields++;

			timeout = 0;
		}
		else
		{
			timeout++;
		}
	}

	return true;
}

// Write processed fields to output stream and tbc.json buffer
bool write_fields()
{
	uint64_t bytesWritten = 0;
	Value& fields = tbcJSONIn["fields"];

	for (uint32_t i = 0; i < outputFieldCount; i++)
	{
		// Write video data

		uint32_t fieldIdx = fieldIdxs[i];
		bytesWritten = fwrite(pFields[fieldIdx], sizeof(uint16_t), 263 * 910, fpOutputStream); // TODO: Check for errors

		// Write json data

		fieldSqNoOut++;

		Value field;
		uint32_t jsonFieldIdx = ((fieldSqNoIn - inputFieldCount) + fieldIdx);
		field.CopyFrom(fields[jsonFieldIdx], tbcJSONOut.GetAllocator());
		field["seqNo"].Set(fieldSqNoOut);

		pTbcFields->PushBack(field, tbcJSONOut.GetAllocator());
	}

	outputFieldCount = 0;

	return true;
}

// Route a field from the input field buffer to output field buffer (by index)
void route_field(uint32_t inputFieldIdx, uint32_t outputFieldIdx)
{
	// TODO: Bounds checking
	fieldIdxs[outputFieldIdx] = inputFieldIdx;
	outputFieldCount++;
}

// Copy extra unprocessed fields to the output buffers
void copy_remaining_fields()
{
	for (uint32_t i = 0; i < inputFieldCount; i++)
	{
		route_field(i, i);
	}
}

// Process command line arguments
//   -ivtc [str]: enable ivtc with pulldown (ex. "3:2" or "4:2")
//   -s    [int]: set starting field
//   -i    [str]: set input stream
//   -o    [str]: set output stream
//   -j    [str]: set input tbc.json file
//   -jo   [str]: set output tbc.json file
bool process_opts(int argc, char* argv[])
{
	cerr << "[TBC-Tools] Processing with arguments: ";
	for (uint32_t i = 1; i < argc; i += 2)
	{
		if (i + 1 < argc)
		{
			cerr << "[" << argv[i] << " " << argv[i + 1] << "] ";
		}
	}

	cerr << endl;

	for (uint32_t i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-ivtc") == 0)
		{
			if (i + 1 <= argc)
			{
				doIVTC = true;
				pulldown = argv[i + 1];
				i++;
			}
			else
			{
				// TODO: Error message
				return false;
			}
		}

		if (strcmp(argv[i], "-s") == 0)
		{
			if (i + 1 <= argc)
			{
				start = atoi(argv[i + 1]);
				i++;
			}
			else
			{
				// TODO: Error message
				return false;
			}
		}

		else if (strcmp(argv[i], "-i") == 0)
		{
			if (i + 1 <= argc)
			{
				inputStreamPath = argv[i + 1];
				i++;
			}
			else
			{
				// TODO: Error message
				return false;
			}
		}

		else if (strcmp(argv[i], "-o") == 0)
		{
			if (i + 1 <= argc)
			{
				outputStreamPath = argv[i + 1];
				tbcJSONpathOut = outputStreamPath;
				tbcJSONpathOut.append(".json");
				i++;
			}
			else
			{
				// TODO: Error message
				return false;
			}
		}

		else if (strcmp(argv[i], "-j") == 0)
		{
			if (i + 1 <= argc)
			{
				tbcJSONpathIn = argv[i + 1];
				i++;
			}
			else
			{
				// TODO: Error message
				return false;
			}
		}

		else if (strcmp(argv[i], "-jo") == 0)
		{
			if (i + 1 <= argc)
			{
				tbcJSONpathOut = argv[i + 1];
				i++;
			}
			else
			{
				// TODO: Error message
				return false;
			}
		}
	}

	return true;
}

// Load original tbc.json file
int load_json()
{
	char* pJsonBufferIn	= nullptr;
	FILE* jsonFileIn	= nullptr;
	long bufferSize		= 0;
	long bytesRead		= 0;
	int result			= 0;

	// Load tbc.json file as a string

	result = fopen_s(&jsonFileIn, tbcJSONpathIn.c_str(), "r");
	if (result == -1 || !jsonFileIn)
	{ 
		cerr << "[TBC-Tools] Could not load tbc.json file [" << tbcJSONpathIn.c_str() << "]. File not found, or is unreadable." << endl;
		return false;
	} 

	result = fseek(jsonFileIn, 0L, SEEK_END);
	if (result != 0)
	{
		cerr << "[TBC-Tools] Could not load tbc.json file [" << tbcJSONpathIn.c_str() << "]. Seek failed." << endl;
		return false;
	}

	bufferSize = ftell(jsonFileIn);
	if (bufferSize == -1)
	{
		cerr << "[TBC-Tools] Could not load tbc.json file [" << tbcJSONpathIn.c_str() << "]. Could not determine file size." << endl;
		return false;
	}

	pJsonBufferIn = new char[bufferSize + 1] {};

	fseek(jsonFileIn, 0L, SEEK_SET);
	bytesRead = fread_s(pJsonBufferIn, bufferSize, sizeof(char), bufferSize, jsonFileIn);

	if (ferror(jsonFileIn) != 0)
	{ 
		delete[] pJsonBufferIn;
		return -1; // read error
	}

	// Parse json file

	tbcJSONIn.Parse(pJsonBufferIn);

	// Copy header info to output 

	tbcJSONOut.SetObject();
	tbcJSONOut.AddMember("pcmAudioParameters", tbcJSONIn["pcmAudioParameters"], tbcJSONOut.GetAllocator());
	tbcJSONOut.AddMember("videoParameters", tbcJSONIn["videoParameters"], tbcJSONOut.GetAllocator());

	Value tbcTools;
	tbcTools.SetObject();
	tbcTools.AddMember("name", Value(TBC_TOOLS_NAME), tbcJSONOut.GetAllocator());
	tbcTools.AddMember("version", Value(TBC_TOOLS_VERSION), tbcJSONOut.GetAllocator());
	tbcTools.AddMember("github", Value(TBC_TOOLS_GITHUB), tbcJSONOut.GetAllocator());

	tbcJSONOut.AddMember("tbcTools", tbcTools, tbcJSONOut.GetAllocator());

	Value tbcFields;
	tbcFields.SetArray();
	tbcJSONOut.AddMember("fields", tbcFields, tbcJSONOut.GetAllocator());
	pTbcFields = &tbcJSONOut["fields"];

	// Clean up

	fclose(jsonFileIn);
	delete[] pJsonBufferIn;

	return true;
}

// Save new tbc.json file
bool save_json()
{
	FILE* jsonFileOut	= nullptr;
	long bytesWritten	= 0;
	int result			= 0;

	cerr << "[TBC-Tools] Saving tbc.json [" << tbcJSONpathOut.c_str() << "]." << endl;

	// Set final field count
	tbcJSONOut["videoParameters"]["numberOfSequentialFields"].Set(fieldSqNoOut);

	result = fopen_s(&jsonFileOut, tbcJSONpathOut.c_str(), "w");
	if (result == -1 || !jsonFileOut)
	{
		cerr << "[TBC-Tools] Could not write tbc.json file [" << tbcJSONpathOut.c_str() << "]." << endl;
		return false;
	} 

	StringBuffer jsonBufferOut;
	Writer<StringBuffer> writer(jsonBufferOut);
	tbcJSONOut.Accept(writer);

	bytesWritten = fwrite(jsonBufferOut.GetString(), sizeof(char), jsonBufferOut.GetSize(), jsonFileOut);
	if (bytesWritten == 0) { return false; } // write error

	fclose(jsonFileOut);

	return true;
}

// Open video input and output streams
bool open_streams()
{
	int result = 0;

	if (strcmp(inputStreamPath.c_str(), "-") == 0)
	{
		fpInputStream = stdin;

		result = _setmode(_fileno(stdin), _O_BINARY);
		if (result == -1)
		{
			cerr << "[TBC-Tools] Failed to set stdin mode to binary." << endl;
			return false;
		}
	}
	else
	{
		result = fopen_s(&fpInputStream, inputStreamPath.c_str(), "rb");

		if (result == -1 || !fpInputStream)
		{ 
			cerr << "[TBC-Tools] Failed to open input stream [" << inputStreamPath.c_str() << "." << endl;
			return false;
		} 
	}

	if (strcmp(outputStreamPath.c_str(), "-") == 0)
	{
		fpOutputStream = stdout;

		result = _setmode(_fileno(stdout), _O_BINARY);
		if (result == -1)
		{
			cerr << "[TBC-Tools] Failed to set stdout mode to binary." << endl;
			return false;
		}
	}
	else
	{
		result = fopen_s(&fpOutputStream, outputStreamPath.c_str(), "wb");

		if (result == -1 || !fpOutputStream)
		{ 
			cerr << "[TBC-Tools] Failed to open output stream [" << outputStreamPath.c_str() << "." << endl;
			return false;
		}
	}

	return true;
}

// Close video input and output streams
void close_streams()
{
	if (fpInputStream) fclose(fpInputStream);
	if (fpOutputStream) fclose(fpOutputStream);
}

// Create field buffers
bool init_buffers()
{
	for (uint32_t i = 0; i < TBC_MAX_FIELDS; i++)
	{
		pFields[i] = new uint16_t[263 * 910]{};
	}

	pPipeBuffer = pFields[0];

	return true;
}

// Delete field buffers
void delete_buffers()
{
	for (uint32_t i = 0; i < TBC_MAX_FIELDS; i++)
	{
		delete[] pFields[i];
	}
}

// Ensure defaults are reasonable
bool check_setup()
{
	if (inputStreamPath.empty())
	{
		cerr << "[TBC-Tools] No input tbc file or stream specified. Use -i [filepath / - ]." << endl;
		return false;
	}
	if (outputStreamPath.empty())
	{
		outputStreamPath = inputStreamPath;
		outputStreamPath.replace(outputStreamPath.end() - 4, outputStreamPath.end(), TBC_OUTPUT_DEFAULT_POSTFIX ".tbc");

		cerr << "[TBC-Tools] No output tbc file or stream specified. Use -o [filepath / - ]. Assuming ["
			<< outputStreamPath.c_str() << "]." << endl;
	}
	if (tbcJSONpathIn.empty())
	{
		tbcJSONpathIn = inputStreamPath;
		tbcJSONpathIn.append(".json");

		cerr << "[TBC-Tools] No input tbc.json file specified. Use -j [filepath]. Assuming ["
			<< tbcJSONpathIn.c_str() << "]." << endl;
	}
	if (tbcJSONpathOut.empty())
	{
		tbcJSONpathOut = outputStreamPath;
		tbcJSONpathOut.append(".json");

		cerr << "[TBC-Tools] No output tbc.json file specified. Use -jo [filepath]. Assuming ["
			<< tbcJSONpathOut.c_str() << "]." << endl;
	}
	if (pulldown.empty())	// Currently the only process, so required field for now.
	{
		pulldown = "3:2";
		doIVTC = true;
		cerr << "[TBC-Tools] No ivtc pulldown specified. Use -ivtc [mode ex. \"3:2\"]. Assuming ["
			<< pulldown.c_str() << "]." << endl;
	}
	if (start == -1)
	{
		start = 0;
		cerr << "[TBC-Tools] No starting field specified. Results may not be correct. Use -s [int]. Assuming ["
			<< to_string(start) << "]." << endl;
	}

	return true;
}

#define DO_TEST 0
#define TEST_NUM "0"

int main(int argc, char* argv[])
{
#if DO_TEST
	
	// test0: -ivtc ?, -s 3?
	// test1: -ivtc 3:2, -s 9

	const char* testArgs[7];
	testArgs[0 ] = "TBC-Tools.exe";
	testArgs[1 ] = "-ivtc";
	testArgs[2 ] = "3:3:4"; 
	testArgs[3 ] = "-s";
	testArgs[4 ] = "9"; 
	testArgs[5 ] = "-i";
	testArgs[6 ] = PROJECT_SRC_DIR "/tbcs/test" TEST_NUM ".tbc";

	argc = sizeof(testArgs) / sizeof(char*);
	argv = const_cast<char**>(testArgs);

#endif

	int result = 0;

	cerr << "[TBC-Tools] Starting." << endl;

	if (!process_opts(argc, argv)) { return 1; }
	if (!check_setup()) { return 1; }
	if (!init_buffers()) { return 1; }
	if (!open_streams()) { return 1; }
	if (!load_json()) { return 1; }

	if (doIVTC)
	{
		setup_pulldown(pulldown);
	}

	// Align field processing with starting field
	if (true)
	{
		accumulate_fields(start);
		copy_remaining_fields();

		if (!write_fields())
		{
			// TODO: error
		}
	}

	// Process fields
	while (true)
	{
		if (accumulate_fields(fieldAccCount))
		{
			// Normal accumulation, process fields

			if (doIVTC)
			{
				pulldown_fields(phase);
			}

			if (!write_fields())
			{
				break; // TODO: error
			}
		}
		else
		{
			// End of stream or error

			copy_remaining_fields();

			if (!write_fields())
			{
				break; // TODO: error
			}

			break;
		}
	}

	if (!save_json()) { return 1; }

	cerr << "[TBC-Tools] Processing complete." << endl;

	close_streams();
	delete_buffers();

#if DO_TEST

	system("ld-analyse.exe " PROJECT_SRC_DIR "/tbcs/test" TEST_NUM "_tbct.tbc");

#endif

	return 0;
}
