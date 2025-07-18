#pragma once

#include "pico/stdlib.h"

const uint32_t snare_sample_len = 1948;
const uint8_t snare_sample_data[] = {
0x81, 0x7f, 0x80, 0x80, 0x7d, 0x86, 0x7f, 0x68, 0x94, 0x91, 0x7f, 0x83, 0x60, 0x62, 0x73, 0x82, 
0x9b, 0xe0, 0xb2, 0x41, 0x31, 0x54, 0x83, 0x9c, 0x92, 0x8b, 0x6d, 0x76, 0x5c, 0x9c, 0x6c, 0x7a, 
0xf2, 0x99, 0x72, 0x8b, 0x79, 0x6b, 0x9c, 0x3c, 0x54, 0x5e, 0xa7, 0x85, 0x24, 0x75, 0x5b, 0xa0, 
0x8f, 0x8b, 0xa0, 0x9d, 0x8b, 0x94, 0xda, 0xaa, 0x9f, 0x88, 0x6b, 0x75, 0x77, 0x64, 0x4e, 0x52, 
0x74, 0x7e, 0x54, 0x2a, 0x3e, 0x7e, 0x8c, 0x92, 0x93, 0x8c, 0x6d, 0x82, 0xbb, 0x9b, 0x94, 0xab, 
0x83, 0x9a, 0x93, 0xc5, 0xcf, 0xc5, 0x94, 0x3e, 0x5f, 0x50, 0x5e, 0x65, 0x95, 0x83, 0x80, 0x77, 
0x56, 0x4f, 0x52, 0x6a, 0x64, 0x66, 0x73, 0x94, 0x8a, 0x76, 0x70, 0x6e, 0x60, 0x62, 0x69, 0x81, 
0x85, 0x90, 0xa3, 0xad, 0xa7, 0xa1, 0xa6, 0x90, 0x9d, 0xa7, 0x97, 0x78, 0x99, 0x9d, 0x8a, 0x82, 
0x72, 0x70, 0x7e, 0x80, 0x72, 0x81, 0x79, 0x6f, 0x5e, 0x76, 0x90, 0x7e, 0x6e, 0x6a, 0x64, 0x5b, 
0x60, 0x64, 0x6f, 0x77, 0x83, 0x8a, 0x91, 0x93, 0x91, 0x92, 0x8b, 0x85, 0x76, 0x80, 0x70, 0x66, 
0x7b, 0x75, 0x6a, 0x73, 0x6d, 0x79, 0x95, 0x82, 0x85, 0x85, 0x88, 0x80, 0x94, 0xaf, 0x86, 0x89, 
0xab, 0x9a, 0x92, 0x92, 0x83, 0x79, 0x6b, 0x6b, 0x70, 0x88, 0x92, 0x99, 0x9b, 0x95, 0x89, 0x7a, 
0x70, 0x65, 0x64, 0x5b, 0x6d, 0x63, 0x65, 0x70, 0x78, 0x89, 0x75, 0x6f, 0x7f, 0x8f, 0x6b, 0x92, 
0x9d, 0x5a, 0x7d, 0x78, 0x62, 0x87, 0x76, 0x7a, 0x90, 0x75, 0x6b, 0x93, 0x8c, 0x6e, 0x78, 0x7e, 
0x86, 0x7e, 0x86, 0x88, 0x82, 0x86, 0x88, 0x87, 0x97, 0x93, 0x9a, 0x92, 0x91, 0x9e, 0x95, 0xa8, 
0x6c, 0x7d, 0x97, 0x73, 0x7f, 0x9e, 0xa5, 0x69, 0x70, 0x6c, 0x65, 0x9f, 0x7d, 0x7c, 0x93, 0x76, 
0x70, 0x86, 0x99, 0xa6, 0x93, 0x72, 0x78, 0x65, 0x7d, 0x6e, 0x64, 0x5e, 0x73, 0x75, 0x58, 0x64, 
0x82, 0x9d, 0x5f, 0x73, 0x73, 0x7c, 0x89, 0x66, 0x6a, 0x87, 0x76, 0x59, 0x6c, 0x7f, 0x7a, 0x5f, 
0x5f, 0x6c, 0x74, 0x7a, 0x85, 0x76, 0x6f, 0x72, 0x6a, 0x7b, 0x96, 0x9c, 0x9d, 0x9f, 0x83, 0x76, 
0x95, 0x97, 0xab, 0xb2, 0x9e, 0x9d, 0x88, 0x8b, 0xaa, 0x87, 0x90, 0x9c, 0x7f, 0x87, 0x8c, 0x7d, 
0x7d, 0x99, 0x90, 0xa4, 0x8b, 0x8b, 0x7f, 0x74, 0x78, 0x7e, 0xb5, 0x9a, 0x88, 0x96, 0x87, 0x6f, 
0x6a, 0x6a, 0x70, 0x73, 0x78, 0x76, 0x73, 0x74, 0x6e, 0x6d, 0x68, 0x51, 0x6c, 0x5e, 0x46, 0x85, 
0x89, 0x73, 0x66, 0x8b, 0x7e, 0x6c, 0x79, 0x5a, 0x88, 0x89, 0x76, 0x59, 0x6d, 0x83, 0x5c, 0x83, 
0x83, 0x75, 0x85, 0x8d, 0x6e, 0x6d, 0x74, 0x7a, 0x85, 0x83, 0x99, 0x8b, 0x77, 0x89, 0x96, 0x7a, 
0x85, 0x8d, 0x8f, 0x96, 0x79, 0x99, 0x86, 0x7d, 0xa8, 0x82, 0x88, 0x75, 0x75, 0x7b, 0x78, 0x80, 
0x7c, 0xa3, 0xa0, 0x88, 0x88, 0x96, 0x92, 0x82, 0x8d, 0x81, 0x96, 0x7c, 0x7f, 0x93, 0x8d, 0x8d, 
0x6b, 0x9e, 0x7d, 0x7e, 0x80, 0x85, 0x91, 0x79, 0x80, 0x78, 0x6a, 0x74, 0x83, 0x69, 0x7d, 0x6b, 
0x70, 0x88, 0x83, 0x77, 0x70, 0x8b, 0x5e, 0x56, 0x73, 0x66, 0x6f, 0x78, 0x73, 0x7c, 0x8c, 0x69, 
0x65, 0x6a, 0x7f, 0x8f, 0x6a, 0x79, 0x6e, 0x77, 0x93, 0x7e, 0x7c, 0x96, 0x9f, 0x86, 0x87, 0x7f, 
0x8f, 0x93, 0x7d, 0x96, 0x9d, 0x85, 0x70, 0x9c, 0x9b, 0x87, 0x96, 0x7c, 0x91, 0x79, 0x63, 0x88, 
0x78, 0x73, 0x82, 0xa1, 0x73, 0x7e, 0xab, 0x7a, 0x85, 0x94, 0x6d, 0x85, 0x76, 0x6a, 0x99, 0x7e, 
0x78, 0x72, 0x85, 0x7a, 0x5c, 0x60, 0x82, 0x85, 0x6e, 0x75, 0x73, 0x74, 0x73, 0x75, 0x85, 0x81, 
0x8b, 0x92, 0x82, 0x7a, 0x6e, 0x75, 0x69, 0x8d, 0x93, 0x7f, 0x80, 0x94, 0x85, 0x87, 0x82, 0x7d, 
0x7d, 0x6f, 0x82, 0x70, 0x8c, 0x78, 0x7f, 0x7b, 0x7a, 0x91, 0x94, 0x89, 0x5b, 0x72, 0x94, 0x94, 
0x7e, 0x8f, 0x6a, 0x79, 0x95, 0x83, 0x6f, 0x6b, 0x95, 0x6f, 0x77, 0x87, 0x80, 0x93, 0x8f, 0x92, 
0x8f, 0x7b, 0x6e, 0x8a, 0xa0, 0x6a, 0x6d, 0x91, 0xa1, 0x82, 0x77, 0x93, 0x72, 0x88, 0x94, 0x7d, 
0x5b, 0x78, 0x92, 0x6b, 0x75, 0x83, 0x77, 0x82, 0x7f, 0x8b, 0x73, 0x63, 0x83, 0x6f, 0x76, 0x7b, 
0x68, 0x6e, 0x93, 0x79, 0x79, 0x90, 0x6e, 0x66, 0x75, 0x8d, 0x83, 0x7b, 0x7a, 0x79, 0x81, 0x90, 
0x92, 0x82, 0x65, 0x6b, 0x79, 0x83, 0x85, 0x74, 0x99, 0x89, 0x7f, 0x88, 0x89, 0x7c, 0x86, 0x8c, 
0x76, 0x7c, 0x91, 0x91, 0x73, 0x9d, 0x97, 0x86, 0x8f, 0x7c, 0x8c, 0x89, 0x7c, 0x83, 0x73, 0x5a, 
0x69, 0x99, 0x94, 0x8b, 0x7f, 0x95, 0x86, 0x76, 0x85, 0x6f, 0x7e, 0x7e, 0x78, 0x6d, 0xa0, 0x70, 
0x77, 0x89, 0x61, 0x8f, 0x7f, 0x66, 0x76, 0x86, 0x77, 0x92, 0x82, 0x77, 0x8d, 0x66, 0x6e, 0x82, 
0x75, 0x81, 0x7b, 0x94, 0x8f, 0x7a, 0x94, 0x75, 0x78, 0x80, 0x86, 0x85, 0x86, 0x7c, 0x64, 0x81, 
0x97, 0x8c, 0x60, 0x8f, 0x80, 0x6a, 0x73, 0x6e, 0x89, 0x61, 0x8a, 0x87, 0x80, 0xa3, 0x86, 0x70, 
0x6d, 0x76, 0x75, 0x76, 0x78, 0x81, 0x75, 0x83, 0x8b, 0x7e, 0x94, 0x86, 0x83, 0x86, 0x70, 0x81, 
0x92, 0x86, 0x86, 0x91, 0x77, 0x8b, 0x80, 0x6a, 0x7b, 0x92, 0x8d, 0x66, 0x9b, 0x87, 0x88, 0x8a, 
0x85, 0x90, 0x61, 0x80, 0x8c, 0x69, 0x82, 0x87, 0x79, 0x92, 0x80, 0x7d, 0x83, 0x9b, 0x72, 0x5b, 
0x95, 0x79, 0x7d, 0x8d, 0x82, 0x78, 0x77, 0x80, 0x8c, 0x79, 0x51, 0x91, 0x87, 0x70, 0x89, 0x7b, 
0x86, 0x8b, 0x74, 0x6c, 0x89, 0x6b, 0x7f, 0x85, 0x7d, 0x76, 0x77, 0x81, 0x6d, 0x76, 0x6c, 0x88, 
0x8b, 0x90, 0x7a, 0x73, 0x79, 0x91, 0x91, 0x65, 0x78, 0x8a, 0x93, 0x73, 0x87, 0x77, 0x8b, 0x82, 
0x74, 0x88, 0x74, 0x83, 0x6a, 0x91, 0x8d, 0x7d, 0x91, 0x7c, 0x6e, 0x91, 0x76, 0x76, 0xa4, 0x81, 
0x7e, 0x92, 0x80, 0x7d, 0x8a, 0x85, 0x77, 0x7a, 0x7a, 0x78, 0x9c, 0x6d, 0x6d, 0x7c, 0x81, 0x87, 
0x80, 0x90, 0x79, 0x89, 0x91, 0x76, 0x6f, 0x8a, 0x93, 0x85, 0x75, 0x77, 0x81, 0x80, 0x7a, 0x82, 
0x68, 0x92, 0x85, 0x60, 0x81, 0x81, 0x88, 0x83, 0x85, 0x6c, 0x82, 0x83, 0x6a, 0x80, 0x91, 0x83, 
0x8b, 0x6f, 0x6e, 0x8c, 0x70, 0x83, 0x88, 0x69, 0x79, 0x7e, 0x7f, 0x80, 0x78, 0x95, 0x85, 0x70, 
0x88, 0x81, 0x89, 0x82, 0x74, 0x80, 0x7d, 0x9b, 0x75, 0x83, 0x95, 0x74, 0x74, 0x77, 0x81, 0x7f, 
0x83, 0x8a, 0x87, 0x89, 0x89, 0x7e, 0x7e, 0x87, 0x7e, 0x70, 0x91, 0x79, 0x78, 0x90, 0x86, 0x7d, 
0x82, 0x76, 0x79, 0x7d, 0x6e, 0x81, 0x80, 0x79, 0x78, 0x8f, 0x70, 0x74, 0x93, 0x77, 0x83, 0x85, 
0x82, 0x82, 0x72, 0x81, 0x79, 0x73, 0x7f, 0x79, 0x7d, 0x8b, 0x81, 0x82, 0x72, 0x75, 0x85, 0x7e, 
0x7e, 0x7e, 0x89, 0x7e, 0x80, 0x81, 0x90, 0x76, 0x73, 0x86, 0x77, 0x87, 0x81, 0x8c, 0x8d, 0x72, 
0x85, 0x8b, 0x8a, 0x7d, 0x7f, 0x8a, 0x87, 0x7c, 0x7a, 0x88, 0x7b, 0x86, 0x82, 0x7a, 0x7c, 0x8f, 
0x7c, 0x73, 0x88, 0x7c, 0x7d, 0x90, 0x8a, 0x74, 0x80, 0x8a, 0x81, 0x76, 0x82, 0x6e, 0x87, 0x7e, 
0x73, 0x8d, 0x80, 0x86, 0x87, 0x79, 0x80, 0x7b, 0x72, 0x8b, 0x6d, 0x80, 0x80, 0x7f, 0x81, 0x7d, 
0x7e, 0x74, 0x7d, 0x76, 0x7e, 0x7c, 0x74, 0x77, 0x89, 0x7c, 0x7e, 0x89, 0x76, 0x78, 0x91, 0x76, 
0x7d, 0x88, 0x81, 0x76, 0x7f, 0x85, 0x7f, 0x8b, 0x6f, 0x7e, 0x88, 0x7a, 0x7a, 0x8c, 0x79, 0x88, 
0x83, 0x79, 0x91, 0x7c, 0x7e, 0x83, 0x82, 0x7e, 0x8a, 0x8c, 0x7a, 0x8d, 0x8c, 0x7d, 0x75, 0x88, 
0x86, 0x77, 0x8a, 0x86, 0x80, 0x79, 0x83, 0x88, 0x83, 0x76, 0x80, 0x81, 0x76, 0x7f, 0x7d, 0x85, 
0x80, 0x7d, 0x7e, 0x76, 0x89, 0x88, 0x76, 0x78, 0x79, 0x80, 0x74, 0x7e, 0x77, 0x7f, 0x80, 0x78, 
0x7f, 0x7e, 0x80, 0x82, 0x77, 0x7e, 0x83, 0x78, 0x83, 0x77, 0x81, 0x7f, 0x7e, 0x7a, 0x77, 0x80, 
0x78, 0x81, 0x83, 0x7c, 0x7c, 0x82, 0x8a, 0x7c, 0x83, 0x85, 0x78, 0x8d, 0x81, 0x7e, 0x8c, 0x82, 
0x83, 0x81, 0x79, 0x8b, 0x7f, 0x83, 0x7b, 0x7e, 0x8b, 0x83, 0x81, 0x7a, 0x82, 0x86, 0x7b, 0x81, 
0x92, 0x7d, 0x7d, 0x83, 0x83, 0x79, 0x77, 0x86, 0x82, 0x81, 0x88, 0x7c, 0x7f, 0x80, 0x6e, 0x79, 
0x89, 0x7f, 0x74, 0x88, 0x82, 0x70, 0x78, 0x87, 0x7a, 0x76, 0x81, 0x80, 0x83, 0x7d, 0x76, 0x87, 
0x7e, 0x79, 0x81, 0x7d, 0x85, 0x7a, 0x7d, 0x81, 0x79, 0x83, 0x82, 0x79, 0x87, 0x77, 0x7f, 0x81, 
0x7c, 0x83, 0x7f, 0x81, 0x7d, 0x83, 0x82, 0x8c, 0x7d, 0x78, 0x88, 0x7d, 0x7e, 0x87, 0x7f, 0x7a, 
0x81, 0x85, 0x86, 0x80, 0x7a, 0x89, 0x86, 0x7e, 0x7f, 0x79, 0x8a, 0x7f, 0x80, 0x7a, 0x82, 0x89, 
0x7e, 0x82, 0x81, 0x7b, 0x7f, 0x85, 0x7c, 0x85, 0x79, 0x79, 0x7c, 0x7d, 0x81, 0x87, 0x79, 0x7c, 
0x85, 0x7c, 0x83, 0x7e, 0x7d, 0x7c, 0x85, 0x7e, 0x81, 0x7e, 0x7f, 0x80, 0x7d, 0x87, 0x76, 0x7c, 
0x86, 0x80, 0x77, 0x7d, 0x7e, 0x80, 0x7a, 0x7d, 0x82, 0x7d, 0x86, 0x7f, 0x7c, 0x81, 0x7e, 0x7e, 
0x81, 0x80, 0x85, 0x78, 0x81, 0x82, 0x80, 0x86, 0x79, 0x7e, 0x85, 0x80, 0x80, 0x85, 0x7d, 0x83, 
0x81, 0x7f, 0x7f, 0x82, 0x86, 0x7e, 0x7b, 0x87, 0x85, 0x78, 0x80, 0x81, 0x81, 0x78, 0x85, 0x82, 
0x79, 0x83, 0x80, 0x7a, 0x7f, 0x86, 0x82, 0x7c, 0x82, 0x83, 0x7d, 0x7d, 0x7f, 0x7d, 0x7e, 0x82, 
0x79, 0x7f, 0x7f, 0x7a, 0x7e, 0x82, 0x7e, 0x7c, 0x81, 0x80, 0x83, 0x7e, 0x7a, 0x85, 0x80, 0x7e, 
0x80, 0x80, 0x79, 0x7f, 0x83, 0x7d, 0x81, 0x7f, 0x7c, 0x7f, 0x7e, 0x80, 0x81, 0x7e, 0x81, 0x7e, 
0x85, 0x7e, 0x80, 0x86, 0x7a, 0x7f, 0x85, 0x7f, 0x7d, 0x80, 0x80, 0x7f, 0x80, 0x87, 0x7d, 0x80, 
0x7f, 0x7d, 0x80, 0x7f, 0x80, 0x7d, 0x82, 0x86, 0x7d, 0x7d, 0x81, 0x80, 0x7f, 0x80, 0x83, 0x7e, 
0x82, 0x81, 0x7a, 0x7d, 0x81, 0x7e, 0x7f, 0x82, 0x7f, 0x7e, 0x80, 0x81, 0x80, 0x82, 0x7a, 0x7f, 
0x82, 0x81, 0x7e, 0x7c, 0x82, 0x7f, 0x7e, 0x7f, 0x80, 0x7e, 0x7e, 0x80, 0x7f, 0x83, 0x7e, 0x7c, 
0x7d, 0x7e, 0x82, 0x7c, 0x7c, 0x83, 0x83, 0x7c, 0x7e, 0x81, 0x81, 0x81, 0x7c, 0x80, 0x85, 0x7f, 
0x7f, 0x7e, 0x81, 0x80, 0x7e, 0x81, 0x81, 0x7e, 0x7e, 0x82, 0x80, 0x7e, 0x7f, 0x80, 0x7d, 0x81, 
0x85, 0x7a, 0x80, 0x81, 0x7e, 0x7f, 0x81, 0x82, 0x7f, 0x7f, 0x7f, 0x82, 0x80, 0x7f, 0x81, 0x80, 
0x81, 0x81, 0x7d, 0x7e, 0x80, 0x7e, 0x81, 0x7f, 0x7f, 0x81, 0x7f, 0x7d, 0x7f, 0x80, 0x80, 0x7e, 
0x7c, 0x7f, 0x7e, 0x80, 0x85, 0x7f, 0x7d, 0x7f, 0x82, 0x7e, 0x7e, 0x81, 0x80, 0x7e, 0x81, 0x81, 
0x7d, 0x80, 0x80, 0x7e, 0x80, 0x81, 0x7d, 0x81, 0x7f, 0x7e, 0x80, 0x82, 0x80, 0x7e, 0x81, 0x80, 
0x81, 0x7f, 0x80, 0x7e, 0x7f, 0x80, 0x7f, 0x7e, 0x7d, 0x82, 0x82, 0x80, 0x7f, 0x7e, 0x81, 0x7e, 
0x7d, 0x82, 0x7e, 0x80, 0x82, 0x81, 0x7f, 0x7f, 0x7f, 0x80, 0x7f, 0x81, 0x81, 0x7d, 0x82, 0x82, 
0x7e, 0x7e, 0x82, 0x80, 0x7d, 0x7e, 0x81, 0x7e, 0x81, 0x80, 0x7f, 0x82, 0x7e, 0x80, 0x80, 0x7f, 
0x81, 0x7e, 0x7e, 0x82, 0x7f, 0x7e, 0x7e, 0x81, 0x7f, 0x7f, 0x82, 0x7d, 0x81, 0x7f, 0x7e, 0x80, 
0x80, 0x7e, 0x80, 0x81, 0x7f, 0x80, 0x7f, 0x80, 0x7e, 0x7f, 0x80, 0x7f, 0x7f, 0x80, 0x80, 0x7e, 
0x80, 0x80, 0x7f, 0x80, 0x7e, 0x7e, 0x80, 0x7f, 0x7d, 0x7f, 0x80, 0x80, 0x7f, 0x7e, 0x81, 0x80, 
0x80, 0x81, 0x7f, 0x81, 0x81, 0x7e, 0x81, 0x7f, 0x7e, 0x80, 0x7f, 0x7f, 0x81, 0x7e, 0x80, 0x80, 
0x7f, 0x81, 0x80, 0x7f, 0x7f, 0x81, 0x80, 0x7e, 0x7f, 0x80, 0x7e, 0x81, 0x7f, 0x7f, 0x80, 0x80, 
0x80, 0x80, 0x7f, 0x7f, 0x80, 0x7e, 0x7f, 0x7f, 0x7e, 0x7d, 0x80, 0x80, 0x80, 0x7f, 0x80, 0x80, 
0x7e, 0x7f, 0x7f, 0x7e, 0x7f, 0x81, 0x7f, 0x7f, 0x80, 0x80, 0x7e, 0x7f, 0x80, 0x7f, 0x7f, 0x7f, 
0x7f, 0x80, 0x7f, 0x7f, 0x81, 0x80, 0x7e, 0x7f, 0x80, 0x7f, 0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x80, 
0x7f, 0x80, 0x7f, 0x7f, 0x81, 0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x80, 0x7f, 0x80, 0x80, 0x80, 0x80, 
0x80, 0x7f, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x80, 0x7f, 0x80, 0x80, 0x7f, 0x80, 0x80, 0x7f, 0x7f, 
0x80, 0x80, 0x7f, 0x7f, 0x80, 0x7f, 0x80, 0x7f, 0x80, 0x80, 0x7e, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 
0x7f, 0x80, 0x80, 0x7f, 0x7f, 0x80, 0x80, 0x7e, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x81, 0x80, 0x7f, 
0x7f, 0x80, 0x80, 0x7e, 0x80, 0x80, 0x7f, 0x7f, 0x81, 0x7f, 0x7f, 0x80, 0x7f, 0x80, 0x7f, 0x7f, 
0x7f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x80, 0x7f, 0x80, 0x7f, 0x80, 0x80, 0x7f, 0x80, 0x7f, 
0x80, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x80, 0x7f, 0x80, 0x81, 0x7f, 0x7e, 0x80, 
0x80, 0x7f, 0x7f, 0x7f, 0x81, 0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x7f, 0x80, 0x7f, 
0x7e, 0x80, 0x80, 0x80, 0x7f, 0x80, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x7f, 0x80, 0x80, 0x80, 0x7f, 
0x80, 0x7f, 0x80, 0x80, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x80, 
0x7f, 0x7f, 0x80, 0x80, 0x7f, 0x80, 0x80, 0x80, 0x7f, 0x80, 0x80, 0x7f, 0x7f, 0x80, 0x80, 0x7f, 
0x7f, 0x80, 0x80, 0x7f, 0x80, 0x7f, 0x80, 0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x80, 0x80, 0x7f, 0x7f, 
0x80, 0x7f, 0x80, 0x7f, 0x80, 0x80, 0x80, 0x7f, 0x80, 0x7f, 0x7f, 0x80, 0x80, 0x7f, 0x7e, 0x7f, 
0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x7f, 
0x80, 0x7f, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x80, 
0x80, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x80, 0x80, 0x7f, 0x80, 0x80, 
0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 
};
