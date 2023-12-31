#include "TBC-Tools.h"

#include <iostream>
#include <string> 
#include <cstdlib>
#include <cstdio>

#include <stdio.h>
#include <fcntl.h>
#include <io.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

#define TBC_TOOLS_NAME		"Nebulous39's TBC-Tools"
#define TBC_TOOLS_GITHUB	"..."
#define TBC_TOOLS_VERSION	"alpha v0.1"

// Video input/output streams
FILE* ffmpegPipeIn		= stdin;
FILE* ffmpegPipeOut		= stdout;

// Raw TBC Buffers (default 910x263)
uint16_t* pPipeBuffer	= nullptr;	
uint16_t* pField[2]		= { nullptr, nullptr };

// Example.tbc.json files
string		tbcJSONpathIn;
string		tbcJSONpathOut;
Document	tbcJSONIn;
Document	tbcJSONOut;
Value*		pTbcFields;

// Counters
uint64_t start			= 0;
uint64_t totalFrames	= 0;
uint64_t frame			= 0;
uint64_t fieldHalf		= 0;
uint64_t fieldSqNoIn	= 0;
uint64_t fieldSqNoOut	= 0;
uint64_t timeout		= 0;

bool doTimeout = true;

void process_field()
{
	if (fieldHalf == 1)
	{
		// Write tbc data

		fwrite(pField[0], sizeof(uint16_t), 263 * 910, ffmpegPipeOut);
		fwrite(pField[1], sizeof(uint16_t), 263 * 910, ffmpegPipeOut);
		fwrite(pField[0], sizeof(uint16_t), 263 * 910, ffmpegPipeOut);
		fwrite(pField[1], sizeof(uint16_t), 263 * 910, ffmpegPipeOut);

		// Write json data

		Value& fields = tbcJSONIn["fields"];

		uint64_t fieldSq = fieldSqNoIn - (fieldSqNoIn % 2);

		Value field0; field0.CopyFrom(fields[fieldSq + 0], tbcJSONOut.GetAllocator());
		Value field1; field1.CopyFrom(fields[fieldSq + 1], tbcJSONOut.GetAllocator());
		Value field2; field2.CopyFrom(fields[fieldSq + 0], tbcJSONOut.GetAllocator());
		Value field3; field3.CopyFrom(fields[fieldSq + 1], tbcJSONOut.GetAllocator());

		field0["seqNo"].Set(fieldSqNoOut + 0);
		field1["seqNo"].Set(fieldSqNoOut + 1);
		field2["seqNo"].Set(fieldSqNoOut + 2);
		field3["seqNo"].Set(fieldSqNoOut + 3);

		pTbcFields->PushBack(field0, tbcJSONOut.GetAllocator());
		pTbcFields->PushBack(field1, tbcJSONOut.GetAllocator());
		pTbcFields->PushBack(field2, tbcJSONOut.GetAllocator());
		pTbcFields->PushBack(field3, tbcJSONOut.GetAllocator());
		
		fieldSqNoOut += 4;
		frame++;
	}

	fieldSqNoIn++;
	fieldHalf = (fieldHalf + 1) % 2;
	pPipeBuffer = pField[fieldHalf];
}

int loadJSON(const char* filepath)
{
	char* pJsonBufferIn	= nullptr;
	FILE* jsonFileIn	= nullptr;
	long bufferSize		= 0;
	long bytesRead		= 0;
	int result			= 0;

	// Load tbc.json file as a string

	result = fopen_s(&jsonFileIn, filepath, "r");
	if (result == -1 || !jsonFileIn) { return -1; } // file error

	result = fseek(jsonFileIn, 0L, SEEK_END);
	if (result != 0) { return -1; } // seek error

	bufferSize = ftell(jsonFileIn);
	if (bufferSize == -1) { return -1; } // error

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

	totalFrames = tbcJSONOut["videoParameters"]["numberOfSequentialFields"].GetInt64();
	tbcJSONOut["videoParameters"]["numberOfSequentialFields"].Set(totalFrames * 2);
	

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

	return 0;
}

int saveJSON(const char* filepath)
{
	FILE* jsonFileOut	= nullptr;
	long bytesWritten	= 0;
	int result			= 0;

	result = fopen_s(&jsonFileOut, filepath, "w");
	if (result == -1 || !jsonFileOut) { return -1; } // file error

	StringBuffer jsonBufferOut;
	Writer<StringBuffer> writer(jsonBufferOut);
	tbcJSONOut.Accept(writer);

	bytesWritten = fwrite(jsonBufferOut.GetString(), sizeof(char), jsonBufferOut.GetSize(), jsonFileOut);
	if (bytesWritten == 0) { return -1; } // write error

	fclose(jsonFileOut);

	return 0;
}

int process_opts(int argc, char* argv[])
{
	for (unsigned int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-s"))
		{
			if (i + 1 <= argc)
			{
				start = atoi(argv[i + 1]);
				i++;
			}
			else
			{
				// error
			}
		}

		else if (strcmp(argv[i], "-i"))
		{
			if (i + 1 <= argc)
			{
				if (!strcmp(argv[i + 1], "-"))
				{
					ffmpegPipeIn = fopen(argv[i + 1], "rb");
					doTimeout = false;
				}

				i++;
			}
			else
			{
				// error
			}
		}

		else if (strcmp(argv[i], "-o"))
		{
			if (i + 1 <= argc)
			{
				if (!strcmp(argv[i + 1], "-"))
				{
					ffmpegPipeOut = fopen(argv[i + 1], "wb");
				}

				i++;
			}
			else
			{
				// error
			}
		}

		else if (strcmp(argv[i], "-j"))
		{
			if (i + 1 <= argc)
			{
				tbcJSONpathIn = argv[i + 1];
			}
			else
			{
				// error
			}
		}
	}

	return 0;
}

int main(int argc, char* argv[])
{
	pField[0] = new uint16_t[263 * 910]{};
	pField[1] = new uint16_t[263 * 910]{};
	pPipeBuffer = pField[0];

	int result = 0;

	if (process_opts(argc, argv))
	{
		return 0;
	}

	if (ffmpegPipeIn == stdin)
	{
		result = _setmode(_fileno(stdin), _O_BINARY);
		if (result == -1)
		{
			cout << "[TBC-IVTC] Could not set stdin mode to binary. Exiting." << endl;
			return 1;
		}
	}

	if (ffmpegPipeOut == stdout)
	{
		result = _setmode(_fileno(stdout), _O_BINARY);
		if (result == -1)
		{
			cout << "[TBC-IVTC] Could not set stdout mode to binary. Exiting." << endl;
			return 1;
		}
	}

#if 0
	{
		ffmpegPipeIn = _popen("ffmpeg -f rawvideo -video_size 910x263 -pix_fmt gray16 -i \"R:\\Ingest 2\\Record of Lodess War\\Record of Lodess War Deedlit.tbc\" -c:v rawvideo -f rawvideo -", "rb");
		ffmpegPipeOut = _popen("ffmpeg -f rawvideo -video_size 910x263 -pix_fmt gray16 -i - -c:v rawvideo -y -f rawvideo \"R:\\Ingest 2\\Record of Lodess War\\Record of Lodess War Deedlit_ivtc.tbc\"", "wb");
		tbcJSONpath = "R:\\Ingest 2\\Record of Lodess War\\Record of Lodess War Deedlit.tbc.json";
	}
#endif

#if 1
	{
		ffmpegPipeIn = _popen("ffmpeg -f rawvideo -video_size 910x263 -pix_fmt gray16 -i \"R:\\Ingest 2\\Record of Lodess War\\Record of Lodoss War OP 3-Stack.tbc\" -c:v rawvideo -f rawvideo -", "rb");
		ffmpegPipeOut = _popen("ffmpeg -f rawvideo -video_size 910x263 -pix_fmt gray16 -i - -c:v rawvideo -y -f rawvideo \"R:\\Ingest 2\\Record of Lodess War\\Record of Lodoss War OP 3-Stack_ivtc.tbc\"", "wb");
		tbcJSONpathIn = "R:\\Ingest 2\\Record of Lodess War\\Record of Lodoss War OP 3-Stack.tbc.json";
		tbcJSONpathOut = "R:\\Ingest 2\\Record of Lodess War\\Record of Lodoss War OP 3-Stack_ivtc.tbc.json";
	}
#endif

	result = loadJSON(tbcJSONpathIn.c_str());
	if (result == -1)
	{
		cout << "[TBC-IVTC] Could not load tbc.json file. Exiting." << endl;
		return 1;
	}

	while (frame < totalFrames - 4)
	{
		if (doTimeout && timeout > 2500000) { break; }

		size_t read = fread(pPipeBuffer, sizeof(uint16_t), 263 * 910, ffmpegPipeIn);

		if (read > 0)
		{
			process_field();
			timeout = 0;
		}
		else
		{
			timeout++;
		}
	}

	result = saveJSON(tbcJSONpathOut.c_str());
	if (result == -1)
	{
		cout << "[TBC-IVTC] Could not write tbc.json file. Exiting." << endl;
		return 1;
	}

	return 0;
}
