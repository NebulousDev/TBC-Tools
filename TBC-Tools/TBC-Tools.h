#pragma once

#include <string> 
#include <cstdlib>

// Set the number of fields to accumulate each proccessing period
void set_field_accumulation(uint32_t fields);

// Route a field from the input field buffer to output field buffer (by index)
void route_field(uint32_t inputFieldIdx, uint32_t outputFieldIdx);