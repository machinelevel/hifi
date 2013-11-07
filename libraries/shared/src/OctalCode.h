//
//  OctalCode.h
//  hifi
//
//  Created by Stephen Birarda on 3/15/13.
//  Copyright (c) 2013 HighFidelity, Inc. All rights reserved.
//

#ifndef __hifi__OctalCode__
#define __hifi__OctalCode__

#include <string.h>
#include <QString>

const int BITS_IN_BYTE  = 8;
const int BITS_IN_OCTAL = 3;
const int NUMBER_OF_COLORS = 3; // RGB!
const int SIZE_OF_COLOR_DATA = NUMBER_OF_COLORS * sizeof(unsigned char); // size in bytes
const int RED_INDEX   = 0;
const int GREEN_INDEX = 1;
const int BLUE_INDEX  = 2;

void printOctalCode(const unsigned char* octalCode);
int bytesRequiredForCodeLength(unsigned char threeBitCodes);
int branchIndexWithDescendant(const unsigned char* ancestorOctalCode, const unsigned char* descendantOctalCode);
unsigned char* childOctalCode(const unsigned char* parentOctalCode, char childNumber);

const int OVERFLOWED_OCTCODE_BUFFER = -1;
const int UNKNOWN_OCTCODE_LENGTH = -2;

/// will return -1 if there is an error in the octcode encoding, or it would overflow maxBytes
/// \param const unsigned char* octalCode the octalcode to decode
/// \param int maxBytes number of bytes that octalCode is expected to be, -1 if unknown
int numberOfThreeBitSectionsInCode(const unsigned char* octalCode, int maxBytes = UNKNOWN_OCTCODE_LENGTH);

unsigned char* chopOctalCode(const unsigned char* originalOctalCode, int chopLevels);
unsigned char* rebaseOctalCode(const unsigned char* originalOctalCode, const unsigned char* newParentOctalCode, 
                               bool includeColorSpace = false);

const int CHECK_NODE_ONLY = -1;
bool isAncestorOf(const unsigned char* possibleAncestor, const unsigned char* possibleDescendent, 
        int descendentsChild = CHECK_NODE_ONLY);

// Note: copyFirstVertexForCode() is preferred because it doesn't allocate memory for the return
// but other than that these do the same thing.
float * firstVertexForCode(const unsigned char* octalCode);
void copyFirstVertexForCode(const unsigned char* octalCode, float* output);

struct VoxelPositionSize {
    float x, y, z, s;
};
void voxelDetailsForCode(const unsigned char* octalCode, VoxelPositionSize& voxelPositionSize);

typedef enum {
    ILLEGAL_CODE = -2,
    LESS_THAN = -1,
    EXACT_MATCH = 0,
    GREATER_THAN = 1
} OctalCodeComparison;

OctalCodeComparison compareOctalCodes(const unsigned char* code1, const unsigned char* code2);

QString octalCodeToHexString(const unsigned char* octalCode);
unsigned char* hexStringToOctalCode(const QString& input);

#endif /* defined(__hifi__OctalCode__) */
